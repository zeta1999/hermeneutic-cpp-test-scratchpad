
This is a portable (CMake) project with the following elements
read PDF file @ instructions/Senior_C___Engineer_Test_Assignment.pdf for context

deps for ~ all
- spdlog
- simdjson
- libPOCO
- doctest 

libs [output], for each lib, add a test lib too using doctest for now 

- lob: Limit Order Book, for now just assume interface - for each price level, bid/ask, I have a list of limit orders, with the usual: sequence number, ordernumber, buy/sell, qty, price, trader id, ...
- aggregator: depends on lob 
  trick: 
  + the loop/loops reading network events sends messages to a shared concurrent queue/FIFO #1 [x threads depending on how tf the libPOCO likes threads]
  + the aggregation loop[s?] then reads events from the concurrent queue #1, and if things need to be published then sends messages to conncurrent queue #2
  + the grpc server related loop/thread reads from #2 and does the streaming to clients
- cex_type1 : a centra exchange mockup - do not try to implement the internal for now
- bbo: lib supporting Publisher for Best Bid-Offer
- volume_bands : => Publisher for Volume Bands Prices for 1M/5M/10M/25M/50M+ notional values bands
- price_bands : => Publisher for Price Bands for BBO+ 50bps/100bps/200bps/500bps/1000bps+

Nota: connection from aggregator to CEX: websockets, make sure you add auth of sort (auth token, etc.)
Note: connection from bbo,volume,price bands to aggregator: use gRPC, with auth and strming based interface

programs [output]
- cex_type1_service : uses cex_type1
- aggregator_service : uses aggregator
  uses GRPC and websocket/http clients [use libPOCO if possible]
- bbo_service : Publisher for Best Bid-Offer
- volume_bands_service : Publisher for Volume Bands Prices for 1M/5M/10M/25M/50M+ notionalvalues bands
- price_bands_service : Publisher for Price Bands for BBO+ 50bps/100bps/200bps/500bps/1000bps+

We must have docker for all services [minimal dockers, say debian or even alpine-based, with switch]
We must have a demo docker compose yml to run ... the demo and some trick to get the logs in a pseudo humna readable form (lines encoded as json?)
