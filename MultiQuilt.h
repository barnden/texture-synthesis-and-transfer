// Multithreadded Quilting

#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Quilt.h"

class MultiQuilt {
private:
    Quilt m_quilt;

    std::queue<Coordinate> m_queue;
    std::mutex m_queue_mtx;
    std::condition_variable m_queue_convar;

    std::deque<int> m_status;
    std::mutex m_status_mtx;
    size_t m_total_completed {};
    bool m_completed {};

    std::mutex m_copy_mtx;

    size_t m_max_chunk_x;
    size_t m_max_chunk_y;

public:
    MultiQuilt(Image const& texture, int width, int height)
        : m_quilt(texture, width, height) {};

    template <size_t flag>
    void create_patch_at(Coordinate quxel, Coordinate max, int K)
    {
        if constexpr (flag == Quilt::SYNTHESIS_RANDOM) {
            auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

            m_quilt.copy_patch(quxel, m_quilt.random_patch());
        } else {
            auto patch = m_quilt.random_overlapping_patch(quxel, K);

            if constexpr (flag == Quilt::SYNTHESIS_SIMPLE) {
                auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

                m_quilt.copy_patch(quxel, patch);
            }

            if constexpr (flag == Quilt::SYNTHESIS_CUT) {
                auto mask = m_quilt.find_mask(quxel, patch, max);
                auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

                m_quilt.copy_patch(quxel, patch, mask);
            }
        }
    }

    bool is_complete(Coordinate patch)
    {
        auto status = false;

        {
            auto lock = std::unique_lock<std::mutex>(m_status_mtx);
            status = m_status[patch.x + patch.y * m_max_chunk_x] == 1;
        }

        return status;
    }

    template <size_t flag>
    void worker(int const K)
    {

        while (true) {
            auto chunk = Coordinate {};

            {
                auto lock = std::unique_lock<std::mutex> { m_queue_mtx };

                m_queue_convar.wait(lock, [this] -> bool {
                    return !m_queue.empty() || m_completed;
                });

                if (m_completed)
                    return;

                chunk = m_queue.front();
                m_queue.pop();
            }

            {
                auto lock = std::unique_lock<std::mutex> { m_status_mtx };

                auto& status = m_status[chunk.x + chunk.y * m_max_chunk_x];

                // Avoid working on in-progress or completed patches
                if (status != -1)
                    continue;

                status = 0;
            }

            auto const quxel = Coordinate {
                chunk.x * m_quilt.m_chunk,
                chunk.y * m_quilt.m_chunk
            };

            auto const boundary = Coordinate {
                std::min(m_quilt.m_quilt.width() - 1, quxel.x + m_quilt.m_patch),
                std::min(m_quilt.m_quilt.height() - 1, quxel.y + m_quilt.m_patch)
            };

            if (!(quxel.x || quxel.y)) {
                auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

                m_quilt.copy_patch(quxel, m_quilt.random_patch());
            } else {
                create_patch_at<flag>(quxel, boundary, K);
            }

            {
                auto lock = std::unique_lock<std::mutex>(m_status_mtx);

                m_status[chunk.x + chunk.y * m_max_chunk_x] = 1;
                m_total_completed++;
            }

#if DBGLN
            std::cout << "[MultiQueue] Finished Q" << chunk << " progress: " << m_total_completed << '/' << m_status.size() << '\n';
#endif

            if (chunk.x < m_max_chunk_x - 1) {
                auto top_right = chunk + Coordinate { 1, -1 };

                if (is_complete(top_right))
                    add_patch(chunk + Coordinate { 1, 0 });
            }

            if (chunk.y < m_max_chunk_y - 1) {
                auto bottom_left = chunk + Coordinate { -1, 1 };

                if (is_complete(bottom_left))
                    add_patch(chunk + Coordinate { 0, 1 });
            }

            if (!chunk.x && chunk.y < m_max_chunk_y - 1) {
                // Chunk along left edge, and not at bottom edge
                add_patch(chunk + Coordinate { 0, 1 });
            }

            if (!chunk.y && chunk.x < m_max_chunk_x - 1) {
                // Chunk along top edge, and not at right edge
                add_patch(chunk + Coordinate { 1, 0 });
            }
        }
    }

    void add_patch(Coordinate patch)
    {
#if DBGLN
        std::cout << "[MultiQueue] Enqueuing Q" << patch << '\n';
#endif

        {
            auto lock = std::unique_lock<std::mutex>(m_queue_mtx);
            m_queue.push(patch);
        }

        m_queue_convar.notify_one();
    }

    void synthesize(int patch_sz, int overlap_sz, int K, int flag)
    {
        assert(patch_sz > overlap_sz);

        m_quilt.m_patch = patch_sz;
        m_quilt.m_overlap = overlap_sz;
        m_quilt.m_chunk = patch_sz - overlap_sz;

        m_max_chunk_y = (m_quilt.m_quilt.height() / m_quilt.m_chunk)
                        + (m_quilt.m_quilt.height() % m_quilt.m_chunk != 0);
        m_max_chunk_x = (m_quilt.m_quilt.width() / m_quilt.m_chunk)
                        + (m_quilt.m_quilt.width() % m_quilt.m_chunk != 0);

        m_status = std::deque<int>(m_max_chunk_x * m_max_chunk_y, -1);

        auto const max_threads = std::thread::hardware_concurrency();
        auto pool = std::vector<std::thread> {};

        for (auto i = 0; i < max_threads; i++) {
            pool.push_back(std::thread([this, flag, K] -> void {
                switch (flag) {
                case Quilt::SYNTHESIS_RANDOM:
                    return this->worker<Quilt::SYNTHESIS_RANDOM>(K);

                case Quilt::SYNTHESIS_SIMPLE:
                    return this->worker<Quilt::SYNTHESIS_SIMPLE>(K);

                default:
                    return this->worker<Quilt::SYNTHESIS_CUT>(K);
                }
            }));
        }

        add_patch({ 0, 0 });

        while (is_busy()) { };

        {
            auto lock = std::unique_lock<std::mutex>(m_queue_mtx);
            m_completed = true;
        }

        m_queue_convar.notify_all();

        for (auto&& thread : pool)
            thread.join();

        pool.clear();
    }

    bool is_busy()
    {
        auto busy = false;

        {
            auto lock = std::unique_lock<std::mutex>(m_status_mtx);

            busy = m_total_completed < m_status.size();
        }

        return busy;
    }

    void write(std::string const& filename) const { m_quilt.write(filename); }
};
