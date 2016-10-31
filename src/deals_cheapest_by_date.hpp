#ifndef SRC_DEALS_CHEAPEST_BY_DATE_HPP
#define SRC_DEALS_CHEAPEST_BY_DATE_HPP

#include <unordered_map>
#include <vector>
#include "deals_query.hpp"
#include "deals_types.hpp"
#include "search_query.hpp"

namespace deals {
//------------------------------------------------------------
// CheapestByDay
//------------------------------------------------------------
class CheapestByDay : public DealsSearchQuery {
 public:
  CheapestByDay(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }

  // implement virtual functions:
  void process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;
  std::vector<i::DealInfo> get_result() final override;

 private:
  std::vector<i::DealInfo> exec_result;
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, i::DealInfo>>
      grouped_destinations_and_dates;
};
}  // namespace deals
#endif