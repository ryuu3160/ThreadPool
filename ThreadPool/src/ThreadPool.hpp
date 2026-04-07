/*+===================================================================
	File: ThreadPool.hpp
	Summary: スレッドプール
			 このスレッドプールはタスクの実行順序を保証しない
	Author: 青木雄一郎
	Date: 2026/4/5 Sun AM 06:33:48 初回作成
===================================================================+*/
#pragma once

// ==============================
//	include
// ==============================
#include "Singleton/Singleton.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

/// <summary>
/// ThreadPoolクラス
/// </summary>
class ThreadPool : public Singleton<ThreadPool>
{
    friend class Singleton<ThreadPool>;

public:
    enum class ShutdownMode
    {
        Drain,     // 受付停止→全タスク完了待ち→終了
        Terminate, // 受付停止→即終了(残タスクは破棄)
    };

public:

    /// <summary>
    /// タスク追加(Shutdown開始後は拒否してfalseを返す)
    /// </summary>
    template <typename T>
    bool AddTask(T &&inTask)
    {
        // Shutdown開始後（受付停止）は必ず拒否
        if(!m_AcceptTasks.load(std::memory_order_acquire))
            return false;

        // pendingは「必ず完了通知が走る」前提で増やす
        m_PendingTasks.fetch_add(1, std::memory_order_relaxed);

        // 例外が出てもpending減算が必ず起きるようにラップ
        auto wrapped = [this, task = std::forward<T>(inTask)]() mutable
            {
                try { task(); }
                catch(...) { /* 必要ならログ */ }
                OnTaskFinished();
            };

        const int idx = GetCurrentThreadIndex();

        if(idx >= 0)
        {
            // ワーカースレッド上なら自分のローカルに積む
            m_Workers[static_cast<std::size_t>(idx)]->PushBack(std::move(wrapped));
            // 自分以外を起こす（work stealing を回しやすくする）
            WakeupAll(idx);
        }
        else
        {
            // 外部スレッド上ならグローバルに積む
            {
                std::unique_lock<std::mutex> lock(m_GlobalMutex);
                m_GlobalTasks.emplace_back(std::move(wrapped));
            }
            // 追加通知
            m_GlobalCond.notify_all();
            WakeupAll(-1);
        }

        return true;
    }

    /// <summary>
    /// (受付停止後に)全タスク完了まで待つ
    /// </summary>
    void WaitUntilIdle();

    /// <summary>
    /// シャットダウン(Drain/Terminate)
    /// </summary>
    void Shutdown(ShutdownMode mode);

    inline bool IsAcceptingTasks() const
    {
        return m_AcceptTasks.load(std::memory_order_acquire);
    }

private:
    class Worker
    {
    public:
        Worker(ThreadPool &pool, int index);
        ~Worker();

        Worker(const Worker &) = delete;
        Worker &operator=(const Worker &) = delete;
        Worker(Worker &&) noexcept = delete;
        Worker &operator=(Worker &&) noexcept = delete;

        void RequestTermination();
        void Join();
        void Wakeup();

        template <typename T>
        void PushBack(T &&task)
        {
            {
                std::unique_lock<std::mutex> lock(m_LocalMutex);
                m_LocalTasks.emplace_back(std::forward<T>(task));
            }
            m_LocalCond.notify_one();
        }

        std::function<void()> Steal();

    private:
        void WorkerProc();

    private:
        ThreadPool &m_Pool;
        const int m_Index;
        std::uint32_t m_PopCounter = 0;

        std::atomic<bool> m_IsRequestedTermination{ false };
        std::thread m_Thread;

        std::deque<std::function<void()>> m_LocalTasks;
        std::mutex m_LocalMutex;
        std::condition_variable m_LocalCond;
    };

private:
    ThreadPool();
    ~ThreadPool();

    // TLSから取得(ワーカースレッド以外は -1)
    int GetCurrentThreadIndex() const;

    std::function<void()> TaskStealOrPull(int inIndex);

    void WakeupAll(int inIndex);

    void OnTaskFinished();

private:
    std::vector<std::unique_ptr<Worker>> m_Workers;

    std::atomic<bool> m_AcceptTasks{ true };
    std::atomic<bool> m_IsRequestedTermination{ false };

    std::atomic<int> m_PendingTasks{ 0 };

    std::deque<std::function<void()>> m_GlobalTasks;
    mutable std::mutex m_GlobalMutex;
    std::condition_variable m_GlobalCond;
};
