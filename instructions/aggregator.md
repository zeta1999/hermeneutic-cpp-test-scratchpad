
we continue the specs improvements
- make sure the recent c++ aggregator lib chnges are reflected in the grpc specs (detailed timestamps ...)
- client side, make sure we test re-connecting when a connection is lost
- just like for the websocket code, we should have one test with the libs and c++ test harness
 ... as well as a test or two with the cex executable, the aggregator executable being run and several checks being made
- make sure the aggregstor works using event queues
 i.e.
  - websockets/cex connecting threads push events to a concurrent queue
  - the aggregator updaye threads reads these evens
  - and then the update events emitted by the aggregator loop are pushed to a second concurrent queue
  - lastly a set of grpc threads read events from the second concurrent queue and diffuse events to subscribers

all must be tested
ensure good code coverage
