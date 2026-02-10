
1/
create a macro HERMENEUTIC_ASSERT_DEBUG that is use like assert() but only run in debug mode or if we add the appropriate option to the cmake 
use this in the code in a few places to encode/enforce some basic requirements, that we describe below in 2/

2/

In ALL LOBs,
including the aggregation 
we want to ensure
- ask > bid
- test_ask > best_bid
asks prices go UP from best bid
bid prices go DOWN from best asks 

use HERMENEUTIC_ASSERT_DEBUG to check all these things and others
- no negative prices
- no bid above best_bid
- no ask below best_ask

3/ make sure ctest is fixed and will not be changed if files in data/ are regenerated 
4/ add extra timestamps to csv outputs : 

  int64 last_feed_timestamp_ns = 7;
  int64 last_local_timestamp_ns = 8;
  int64 min_feed_timestamp_ns = 9;
  int64 max_feed_timestamp_ns = 10;
  int64 min_local_timestamp_ns = 11;
  int64 max_local_timestamp_ns = 12;
  int64 publish_timestamp_ns = 13;

5/ add some soft error/warning checking in spdlog if min/max feed diff is too big etc. i.e. alter when feed is not very fresh
