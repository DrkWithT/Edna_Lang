module;

#include <utility>
#include <string>
#include <string_view>
#include <vector>

export module edna.runtime.string_pool;

namespace Edna::Runtime {
    export class StringPool {
    public:
        static constexpr std::size_t string_overhead = sizeof(std::string);
        static constexpr int dud_id = -1;

    private:
        std::vector<std::string> m_items;
        std::vector<int> m_free_list;
        std::size_t m_max_overhead;
        std::size_t m_overhead;
        int m_tenure_count;
        int m_next_id;
        int m_max_id;

        [[nodiscard]] constexpr int try_use_id() noexcept {
            if (!m_free_list.empty()) {
                const int reused_id = m_free_list.back();

                m_free_list.pop_back();

                return reused_id;
            }

            auto next_id = m_next_id;

            if (next_id >= m_max_id) {
                return dud_id;
            }

            m_next_id++;

            return next_id;
        }

    public:
        constexpr StringPool(std::size_t str_capacity)
        : m_items {}, m_free_list {}, m_max_overhead {(str_capacity * string_overhead * 2) / 3}, m_overhead {}, m_tenure_count {}, m_next_id {0}, m_max_id {} {
            m_items.reserve(str_capacity);
            m_items.resize(str_capacity);
            m_max_id = static_cast<int>(str_capacity);
        }

        constexpr void tenure_preloads() noexcept {
            m_tenure_count = m_next_id;
        }

        [[nodiscard]] constexpr bool needs_gc() const noexcept {
            return m_overhead >= m_max_overhead;
        }

        [[nodiscard]] const std::vector<std::string>& cells() const noexcept {
            return m_items;
        }

        [[nodiscard]] int store(std::string s) noexcept {
            const auto result_id = try_use_id();

            if (result_id != dud_id) {
                m_items[result_id] = std::move(s);
                m_overhead += string_overhead;
            }

            return result_id;
        }

        [[nodiscard]] constexpr std::string_view at(int str_id) const noexcept {
            if (str_id < 0 || str_id >= m_max_id) {
                return {""};
            }

            return m_items[str_id];
        }

        constexpr void destroy_at(int str_id) {
            if (str_id >= m_tenure_count && str_id < m_max_id) {
                m_cells[str_id].clear();
                m_free_list.push_back(str_id);
                m_overhead -= string_overhead;
            }
        }
    };
}
