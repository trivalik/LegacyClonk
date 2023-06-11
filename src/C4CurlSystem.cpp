/*
 * LegacyClonk
 *
 * Copyright (c) 2023, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

#include "C4Awaiter.h"
#include "C4CurlSystem.h"
#include "C4Log.h"
#include "C4ThreadPool.h"
#include "StdApp.h"
#include "StdResStr2.h"

#include <format>
#include <ranges>
#include <utility>

#define CURL_STRICTER
#include <curl/curl.h>

template<typename T, typename... Args> requires (sizeof...(Args) >= 1)
static decltype(auto) ThrowIfFailed(T &&result, Args &&...args)
{
	if (!result)
	{
		if constexpr (sizeof...(Args) == 1)
		{
			throw std::runtime_error{std::forward<Args>(args)...};
		}
		else
		{
			throw C4CurlSystem::Exception{std::format(std::forward<Args>(args)...)};
		}
	}

	return std::forward<T>(result);
}

void C4CurlSystem::CURLMultiDeleter::operator()(CURLM *const multi)
{
	curl_multi_cleanup(multi);
}

void C4CurlSystem::CURLEasyDeleter::operator()(CURL *const easy)
{
	curl_easy_cleanup(easy);
}

C4CurlSystem::GlobalInit::GlobalInit()
{
	if (const auto ret = curl_global_init(CURL_GLOBAL_ALL); ret != CURLE_OK)
	{
		std::string message{std::vformat(LoadResStr("IDS_ERR_CURLGLOBALINIT"), std::make_format_args(curl_easy_strerror(ret)))};
		LogF("%s", message.c_str());
		throw CStdApp::StartupException{std::move(message)};
	}
}

C4CurlSystem::GlobalInit::~GlobalInit()
{
	curl_global_cleanup();
}

C4CurlSystem::Awaiter::Awaiter(C4CurlSystem &system, EasyHandle &&easyHandle)
	: system{system},
	  easyHandle{std::move(easyHandle)},
	  result{std::unexpected{std::string(static_cast<std::size_t>(CURL_ERROR_SIZE), '\0')}}
{
	curl_easy_setopt(std::get<0>(this->easyHandle).get(), CURLOPT_ERRORBUFFER, result.error().data());
}

void C4CurlSystem::Awaiter::Resume()
{
	C4ThreadPool::Global->SubmitCallback(coroutineHandle.load(std::memory_order_acquire));
}

C4CurlSystem::AddedEasyHandle::AddedEasyHandle(C4CurlSystem &system, EasyHandle &&easyHandle)
	: system{system}, easyHandle{std::move(easyHandle)}
{
}

C4CurlSystem::AddedEasyHandle::~AddedEasyHandle()
{
	system.get().RemoveHandle(get());
}

C4CurlSystem::C4CurlSystem()
	: multiHandle{curl_multi_init()}
{
	if (!multiHandle)
	{
		std::string message{std::vformat(LoadResStr("IDS_ERR_CURLGLOBALINIT"), std::make_format_args("curl_multi_init failed"))};
		LogF("%s", message.c_str());
		throw CStdApp::StartupException{std::move(message)};
	}

	curl_multi_setopt(multiHandle.get(), CURLMOPT_SOCKETFUNCTION, &C4CurlSystem::SocketFunction);
	curl_multi_setopt(multiHandle.get(), CURLMOPT_SOCKETDATA, this);

	curl_multi_setopt(multiHandle.get(), CURLMOPT_TIMERFUNCTION, &C4CurlSystem::TimerFunction);
	curl_multi_setopt(multiHandle.get(), CURLMOPT_TIMERDATA, this);

	multiTask = Execute();
}

C4CurlSystem::AddedEasyHandle C4CurlSystem::AddHandle(Awaiter &awaiter, EasyHandle &&easyHandle)
{
	AddedEasyHandle addedEasyHandle{*this, std::move(easyHandle)};
	const std::scoped_lock lock{socketMapMutex, awaiterMutex};

	ThrowIfFailed(sockets.try_emplace(addedEasyHandle.get(), std::unordered_map<SOCKET, int>{}).second, "already added");

	if (!awaiters.try_emplace(addedEasyHandle.get(), std::ref(awaiter)).second)
	{
		sockets.erase(easyHandle.get());
		ThrowIfFailed(false, "could not add awaiter");
	}

	ThrowIfFailed(curl_multi_add_handle(multiHandle.get(), addedEasyHandle.get()) == CURLM_OK, "curl_multi_add_handle");

	CancelWait();

	return addedEasyHandle;
}

void C4CurlSystem::RemoveHandle(CURL *const handle)
{
	{
		const std::scoped_lock lock{socketMapMutex, awaiterMutex};

		sockets.erase(handle);
		awaiters.erase(handle);
	}

	curl_multi_remove_handle(multiHandle.get(), handle);

	CancelWait();
}

C4CurlSystem::~C4CurlSystem()
{
	if (multiTask)
	{
		multiTask.Cancel();

		try
		{
			std::move(multiTask).Get();
		}
		catch (const C4Task::CancelledException &)
		{
		}
	}
}

C4Task::Hot<void> C4CurlSystem::Execute()
{
	int running{0};

	curl_multi_socket_action(multiHandle.get(), CURL_SOCKET_TIMEOUT, 0, &running);

	C4Task::Promise<void> &promise{co_await C4Task::GetPromise()};

	for (;;)
	{
		WaitReturnType result{};

		try
		{
			result = co_await Wait();
		}
		catch (const C4Task::CancelledException &)
		{
			if (promise.IsCancelled())
			{
				co_return;
			}
		}

#ifdef _WIN32
		if (result)
		{
			// copy map to prevent crashes
			const auto localSockets = GetSocketMapCopy();

			for (const auto socket : localSockets | std::views::values | std::views::join | std::views::keys)
			{
				if (WSANETWORKEVENTS networkEvents; !WSAEnumNetworkEvents(socket, event.GetEvent(), &networkEvents))
				{
					int eventBitmask{0};
					if (networkEvents.lNetworkEvents & (FD_READ | FD_ACCEPT | FD_CLOSE))
					{
						eventBitmask |= CURL_CSELECT_IN;
					}

					if (networkEvents.lNetworkEvents & (FD_WRITE | FD_CONNECT))
					{
						eventBitmask |= CURL_CSELECT_OUT;
					}

					curl_multi_socket_action(multiHandle.get(), socket, eventBitmask, &running);
				}
			}
		}
#else
		if (!result.empty())
		{
			for (const auto event : result)
			{
				int eventBitmask{0};
				if (event.revents & POLLIN)
				{
					eventBitmask |= CURL_CSELECT_IN;
				}

				if (event.revents & POLLOUT)
				{
					eventBitmask |= CURL_CSELECT_OUT;
				}

				curl_multi_socket_action(multiHandle.get(), event.fd, eventBitmask, &running);
			}
		}
#endif
		else
		{
			curl_multi_socket_action(multiHandle.get(), CURL_SOCKET_TIMEOUT, 0, &running);
		}

		ProcessMessages();
	}
}

C4Task::Cold<C4CurlSystem::WaitReturnType> C4CurlSystem::Wait()
{
	const struct Cleanup
	{
		Cleanup(C4Task::Promise<WaitReturnType> &promise, std::atomic<C4Task::Promise<WaitReturnType> *> &wait)
			: promise{promise}, wait{wait}
		{
			wait.store(&promise, std::memory_order_release);
		}

		~Cleanup()
		{
			C4Task::Promise<WaitReturnType> *expected;

			do
			{
				expected = &promise;
			}
			while (!wait.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel));
		}

		C4Task::Promise<WaitReturnType> &promise;
		std::atomic<C4Task::Promise<WaitReturnType> *> &wait;
	} cleanup{co_await C4Task::GetPromise(), wait};

#ifdef _WIN32
	co_return co_await C4Awaiter::ResumeOnSignal(event.GetEvent(), timeout.load(std::memory_order_acquire));
#else
	co_return co_await C4Awaiter::ResumeOnSignals(
		GetSocketMapCopy()
			| std::views::values
			| std::views::join
			| std::views::transform([](const auto &pair) { return pollfd{.fd = pair.first, .events = static_cast<short>(pair.second)}; }),
		timeout.load(std::memory_order_acquire)
		);
#endif
}

void C4CurlSystem::ProcessMessages()
{
	CURLMsg *message;
	do
	{
		int messagesInQueue{0};
		message = curl_multi_info_read(multiHandle.get(), &messagesInQueue);

		if (message && message->msg == CURLMSG_DONE)
		{
			if (message->data.result != CURLE_OK)
			{
				const std::lock_guard lock{awaiterMutex};

				if (const auto it = awaiters.find(message->easy_handle); it != awaiters.end())
				{
					it->second.get().SetErrorMessage(curl_easy_strerror(message->data.result));
				}
			}

			{
				const std::lock_guard lock{socketMapMutex};
				sockets.erase(message->easy_handle);
			}

			decltype(awaiters)::node_type type;

			{
				const std::lock_guard lock{awaiterMutex};
				type = awaiters.extract(message->easy_handle);
			}

			if (type)
			{
				Awaiter &awaiter{type.mapped().get()};

				char *ip;
				if (curl_easy_getinfo(message->easy_handle, CURLINFO_PRIMARY_IP, &ip) == CURLE_OK)
				{
					C4NetIO::addr_t serverAddress;
					serverAddress.SetHost(StdStrBuf{ip});
					awaiter.SetResult(std::move(serverAddress));
				}
				else
				{
					awaiter.SetErrorMessage("curl_easy_getinfo(CURLINFO_PRIMARY_IP) failed");
				}

				awaiter.Resume();
			}
		}
	}
	while (message);
}

void C4CurlSystem::CancelWait()
{
	if (auto *const promise = wait.exchange(nullptr, std::memory_order_acq_rel); promise)
	{
		const struct Cleanup
		{
			~Cleanup()
			{
				wait.store(promise, std::memory_order_release);
			}

			C4Task::Promise<WaitReturnType> *promise;
			std::atomic<C4Task::Promise<WaitReturnType> *> &wait;
		} cleanup{promise, wait};

		promise->Cancel();
	}
}

int C4CurlSystem::SocketFunction(CURL *const curl, const curl_socket_t s, const int what, void *const userData)
{
	auto &that = *reinterpret_cast<C4CurlSystem *>(userData);

	std::int32_t networkEvents;
#ifdef _WIN32
	static constexpr long NetworkEventsIn{FD_READ | FD_ACCEPT | FD_CLOSE};
	static constexpr long NetworkEventsOut{FD_WRITE | FD_CONNECT};
#else
	static constexpr std::int32_t NetworkEventsIn{POLLIN};
	static constexpr std::int32_t NetworkEventsOut{POLLOUT};
#endif

	switch (what)
	{
	case CURL_POLL_IN:
		networkEvents = NetworkEventsIn;
		break;

	case CURL_POLL_OUT:
		networkEvents = NetworkEventsOut;
		break;

	case CURL_POLL_INOUT:
		networkEvents = NetworkEventsIn | NetworkEventsOut;
		break;

	default:
		networkEvents = 0;
		break;
	}

#ifdef _WIN32
	if (WSAEventSelect(s, that.event.GetEvent(), networkEvents) == SOCKET_ERROR)
	{
		return CURL_SOCKOPT_ERROR;
	}
#endif

	const std::lock_guard lock{that.socketMapMutex};

	if (what == CURL_POLL_REMOVE)
	{
		if (const auto it = that.sockets.find(curl); it != that.sockets.end())
		{
			it->second.erase(s);
		}
	}
	else
	{
		that.sockets.find(curl)->second.insert_or_assign(s, networkEvents);
	}

	return 0;
}

int C4CurlSystem::TimerFunction(CURLM *, const long timeout, void *const userData)
{
	reinterpret_cast<C4CurlSystem *>(userData)->timeout.store(static_cast<std::uint32_t>(std::clamp(timeout, -1L, static_cast<long>(std::numeric_limits<std::int32_t>::max()))), std::memory_order_release);
	return 0;
}
