#pragma once

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Image.h"
#include "Utility.h"

class MultiQuilt;

class Quilt {
protected:
    Image const& m_texture;
    Image m_quilt;

    int m_patch;
    int m_overlap;
    int m_chunk;

    std::vector<std::thread> m_pool;

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

    friend class MultiQuilt;

public:
    static constexpr int SYNTHESIS_RANDOM = 1;
    static constexpr int SYNTHESIS_SIMPLE = 2;
    static constexpr int SYNTHESIS_CUT = 3;
    static constexpr bool SSD_USE_ADDITION = false;
    static constexpr bool SSD_USE_SUBTRACTION = true;
    static constexpr bool VERTICAL_SEAM = true;
    static constexpr bool HORIZONTAL_SEAM = false;

    Quilt(Image const& texture, int width, int height)
        : m_quilt(width, height)
        , m_texture(texture)
    {
        m_queue.push({ 0, 0 });
    }

    [[gnu::flatten]] void copy_patch(Coordinate quilt, Coordinate texture)
    {
        auto max_y = std::min(m_quilt.height(), quilt.y + m_patch);
        auto max_x = std::min(m_quilt.width(), quilt.x + m_patch);

        for (auto i = 0; i < max_x - quilt.x; i++)
            for (auto j = 0; j < max_y - quilt.y; j++) {
                auto offset = Coordinate { i, j };
                m_quilt[quilt + offset] = m_texture[texture + offset];
            }
    }

    template <typename T>
    [[gnu::flatten, gnu::hot]] void copy_patch(Coordinate quilt, Coordinate texture, multivec<T> mask)
    {
        auto max_y = std::min(m_quilt.height(), quilt.y + m_patch);
        auto max_x = std::min(m_quilt.width(), quilt.x + m_patch);

        for (auto i = 0; i < max_x - quilt.x; i++)
            for (auto j = 0; j < max_y - quilt.y; j++) {
                if (!mask[i, j])
                    continue;

                auto offset = Coordinate { i, j };
                m_quilt[quilt + offset] = m_texture[texture + offset];
            }
    }

    Coordinate random_patch() const
    {
        auto p = random(m_texture.width() - m_patch);
        auto q = random(m_texture.height() - m_patch);

        return { p, q };
    }

    template <bool use_subtraction, typename Metric>
    [[gnu::always_inline]] void compute_metric(
        Metric&& metric, auto& ssd,
        auto const& quxel, auto const& patch,
        auto const max_u, auto const max_v) const
    {
        for (auto u = 0; u < max_u; u++) {
            for (auto v = 0; v < max_v; v++) {
                auto const value = metric(quxel, patch, { u, v });

                if constexpr (use_subtraction) {
                    ssd -= value;
                } else {
                    ssd += value;
                }
            }
        }
    }

    template <bool use_subtraction, typename Metric>
    [[gnu::always_inline]] void compute_ssd(
        Metric&& metric, auto& ssd,
        auto const& quxel, auto const& patch,
        auto const init_u, auto const init_v) const
    {
        auto const max_u = std::min(init_u, m_quilt.width() - quxel.x);
        auto const max_v = std::min(init_v, m_quilt.height() - quxel.y);

        compute_metric<use_subtraction>(metric, ssd, quxel, patch, max_u, max_v);
    }

    template <typename Metric>
    [[gnu::flatten]] Coordinate random_overlapping_patch(Coordinate const& quxel, int K, Metric&& compute_metric) const
    {
        auto const top_overlap = quxel.y >= m_chunk;
        auto const left_overlap = quxel.x >= m_chunk;
        auto const corner_overlap = left_overlap && top_overlap;

        auto queue = std::priority_queue<SSD, std::vector<SSD>, std::less<SSD>> {};

        for (auto x = 0; x < m_texture.width() - m_patch; x++)
            for (auto y = 0; y < m_texture.height() - m_patch; y++) {
                auto patch = Coordinate { x, y };
                auto ssd = 0;

                if (left_overlap)
                    compute_ssd<SSD_USE_ADDITION>(compute_metric, ssd, quxel, patch, m_overlap, m_patch);

                if (top_overlap)
                    compute_ssd<SSD_USE_ADDITION>(compute_metric, ssd, quxel, patch, m_patch, m_overlap);

                if (corner_overlap)
                    compute_ssd<SSD_USE_SUBTRACTION>(compute_metric, ssd, quxel, patch, m_overlap, m_overlap);

                if (queue.size() < K || queue.top().ssd > ssd) {
                    if (queue.size() == K)
                        queue.pop();

                    queue.push(SSD { ssd, patch });
                }
            }

        auto it = random(queue.size() - 1);
        for (auto i = 0; i < it; i++)
            queue.pop();

#ifdef DBGLN
        std::cout << "Match [badness: " << queue.top().ssd << "] Texture" << queue.top().coord << " -> Quilt" << quxel << '\n';
#endif

        return queue.top().coord;
    }

    [[gnu::flatten]] Coordinate random_overlapping_patch(Coordinate const& quxel, int K) const
    {
        auto compute_ssd = [this](Coordinate const& quxel, Coordinate const& texel, Coordinate const& coord) {
            auto const& texture = m_texture[texel + coord];
            auto const& quilt = m_quilt[quxel + coord];

            return squared_difference(texture, quilt);
        };

        return random_overlapping_patch(quxel, K, compute_ssd);
    }

    template <bool vertical_seam>
    [[gnu::flatten]] std::vector<Coordinate> find_seam(
        Coordinate quxel,
        Coordinate texel,
        Coordinate overlap) const
    {
        auto max_quxel = quxel + overlap;
        auto max_texel = texel + overlap;

        max_quxel.x = std::min(max_quxel.x, m_quilt.width());
        max_quxel.y = std::min(max_quxel.y, m_quilt.height());

        auto seam_height = 0;
        auto seam_width = 0;

        if constexpr (vertical_seam) {
            seam_height = max_quxel.y - quxel.y;
            seam_width = max_quxel.x - quxel.x;
        } else {
            seam_height = max_quxel.x - quxel.x;
            seam_width = max_quxel.y - quxel.y;
        }

        if (seam_height == 0 || seam_width == 0)
            return {};

        auto seam = std::vector<Coordinate>(seam_height);

        auto energy = std::vector<std::vector<uint64_t>>(seam_height, std::vector<uint64_t>(seam_width, 0));
        auto matrix = std::vector<std::vector<std::pair<int, int>>>(seam_height, std::vector<std::pair<int, int>>(seam_width, std::make_pair(0, 0)));

        for (auto i = 0; i < seam_height; i++) {
            for (auto j = 0; j < seam_width; j++) {
                auto coord = vertical_seam ? Coordinate { j, i } : Coordinate { i, j };

                auto texture = m_texture[texel + coord];
                auto quilt = m_quilt[quxel + coord];

                energy[i][j] = squared_difference(quilt, texture);
            }
        }
        for (auto j = 0; j < seam_width; j++)
            matrix[0][j].first = energy[0][j];

        for (auto i = 1; i < seam_height; i++) {
            auto const& prev_row = matrix[i - 1];

            for (auto j = 0; j < seam_width; j++) {
                auto minval = prev_row[j].first;
                auto k = 0;

                if (j - 1 > 0 && prev_row[j - 1].first < minval) {
                    minval = prev_row[j - 1].first;
                    k = -1;
                }

                if (j + 1 < seam_width && prev_row[j + 1].first < minval) {
                    minval = prev_row[j + 1].first;
                    k = 1;
                }

                matrix[i][j] = std::make_pair(minval + energy[i][j], j + k);
            }
        }

        auto least_energy_element = std::min_element(
            matrix.back().cbegin(), matrix.back().cend(),
            [](auto const& a, auto const& b) { return a.first < b.first; });

        seam[seam_height - 1] = Coordinate {
            static_cast<int>(std::distance(matrix.back().cbegin(), least_energy_element)),
            seam_height - 1
        };

        if (!vertical_seam) {
            auto& back = seam[seam_height - 1];
            std::swap(back.x, back.y);
        }

        for (auto i = seam_height - 1; i-- > 0;) {
            auto j = least_energy_element->second;
            seam[i] = vertical_seam ? Coordinate { j, i } : Coordinate { i, j };

            least_energy_element = matrix[i].cbegin() + j;
        }

        return seam;
    }

    [[gnu::flatten, gnu::hot]] auto find_mask(Coordinate quxel, Coordinate texel, Coordinate max) const
    {
        auto mask = multivec<u_char>(m_patch, m_patch, 1);

        auto mask_seam = [&]<bool B>(Coordinate overlap) {
            auto seam = find_seam<B>(quxel, texel, overlap);

            if (seam.size())
                for (auto&& pixel : seam) {
                    auto max = B ? pixel.x : pixel.y;

                    for (auto i = 0; i <= max; i++) {
                        if constexpr (B) {
                            mask[i, pixel.y] = 0;
                        } else {
                            mask[pixel.x, i] = 0;
                        }
                    }
                }
        };

        auto delta = max - quxel;

        if (quxel.x >= m_chunk)
            mask_seam.template operator()<VERTICAL_SEAM>({ m_overlap, delta.y });

        if (quxel.y >= m_chunk)
            mask_seam.template operator()<HORIZONTAL_SEAM>({ delta.x, m_overlap });

        return mask;
    }

    template <size_t flag>
    [[gnu::hot]] void create_patch_at(Coordinate quxel, Coordinate max, int K)
    {
        if constexpr (flag == Quilt::SYNTHESIS_RANDOM) {
            auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

            copy_patch(quxel, random_patch());
        } else {
            auto patch = random_overlapping_patch(quxel, K);

            if constexpr (flag == Quilt::SYNTHESIS_SIMPLE) {
                auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

                copy_patch(quxel, patch);
            }

            if constexpr (flag == Quilt::SYNTHESIS_CUT) {
                auto mask = find_mask(quxel, patch, max);
                auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

                copy_patch(quxel, patch, mask);
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
                chunk.x * m_chunk,
                chunk.y * m_chunk
            };

            auto const boundary = Coordinate {
                std::min(m_quilt.width() - 1, quxel.x + m_patch),
                std::min(m_quilt.height() - 1, quxel.y + m_patch)
            };

            if (!(quxel.x || quxel.y)) {
                auto copy_lock = std::unique_lock<std::mutex>(m_copy_mtx);

                copy_patch(quxel, random_patch());
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

                if (is_patch_complete(top_right))
                    add_patch(chunk + Coordinate { 1, 0 });
            }

            if (chunk.y < m_max_chunk_y - 1) {
                auto bottom_left = chunk + Coordinate { -1, 1 };

                if (is_patch_complete(bottom_left))
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

    void synthesize(int patch_sz, int overlap_sz, int K, int flag = SYNTHESIS_CUT)
    {
        assert(patch_sz > overlap_sz);

        m_patch = patch_sz;
        m_overlap = overlap_sz;
        m_chunk = patch_sz - overlap_sz;

        m_max_chunk_y = (m_quilt.height() / m_chunk) + (m_quilt.height() % m_chunk != 0);
        m_max_chunk_x = (m_quilt.width() / m_chunk) + (m_quilt.width() % m_chunk != 0);

        m_status = std::deque<int>(m_max_chunk_x * m_max_chunk_y, -1);

        auto const max_threads = std::thread::hardware_concurrency();
        m_pool = decltype(m_pool) {};

        for (auto i = 0; i < max_threads; i++) {
            m_pool.push_back(std::thread([this, flag, K] -> void {
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

        while (is_busy()) { };

        cleanup();
    }

    bool is_patch_complete(Coordinate patch)
    {
        auto status = false;

        {
            auto lock = std::unique_lock<std::mutex>(m_status_mtx);
            status = m_status[patch.x + patch.y * m_max_chunk_x] == 1;
        }

        return status;
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

    void cleanup()
    {
        {
            auto lock = std::unique_lock<std::mutex>(m_queue_mtx);
            m_completed = true;
        }

        m_queue_convar.notify_all();

        for (auto&& thread : m_pool)
            thread.join();

        m_pool.clear();
    }

    void write(std::string const& filename) const { m_quilt.write(filename); }
};
