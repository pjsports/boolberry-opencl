// Copyright (c) 2012-2013 The XXX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "currency_core/currency_format_utils.h"

using namespace currency;

TEST(scratchpad_tests, test_patch)
{
  std::vector<crypto::hash> scratchpad;

  account_base acc;
  acc.generate();
  block b = AUTO_VAL_INIT(b);
  
  std::vector<std::pair<block, std::vector<crypto::hash> > > s;
  for(size_t i = 0; i != 40; i++)
  {
    construct_miner_tx(0, 0, 0, 10000, 0, acc.get_keys().m_account_address, b.miner_tx);
    s.push_back(std::pair<block, std::vector<crypto::hash> >(b, scratchpad));
    push_block_scratchpad_data(b, scratchpad);
  }

  for(auto it = s.rbegin(); it!=s.rend(); it++)
  {
    pop_block_scratchpad_data(it->first, scratchpad);
    ASSERT_EQ(scratchpad, it->second);
  }
}

TEST(scratchpad_tests, test_alt_patch)
{
  std::vector<crypto::hash> scratchpad;

  account_base acc;
  acc.generate();

  std::vector<std::pair<block, std::vector<crypto::hash> > > s;
  for(size_t i = 0; i != 40; i++)
  {
    block b = AUTO_VAL_INIT(b);
    construct_miner_tx(0, 0, 0, 10000, 0, acc.get_keys().m_account_address, b.miner_tx);
    s.push_back(std::pair<block, std::vector<crypto::hash> >(b, scratchpad));
    push_block_scratchpad_data(b, scratchpad);
  }

  std::map<uint64_t, crypto::hash> patch;

  for(uint64_t i = 20; i != s.size(); i++)
  {
    std::vector<crypto::hash> block_addendum;
    bool res = get_block_scratchpad_addendum(s[i].first, block_addendum);
    CHECK_AND_ASSERT_MES(res, void(), "Failed to get_block_scratchpad_addendum for alt block");

    get_scratchpad_patch(s[i].second.size(), 
      0, 
      block_addendum.size(), 
      block_addendum, 
      patch);
  }
  apply_scratchpad_patch(scratchpad, patch);
  for(size_t i = 0; i!=s[20].second.size(); i++)
  {
    ASSERT_EQ(scratchpad[i], s[20].second[i]);
  }

}
