
now for the CEX protocol, what you are building is ***
we need the following, at the very least

- new order: side, volume, [ticker okay to omit if we suppose this is only wss subscribed], order id, sequence number
- cancel order: order id, sequence number
- snapshot: with sequence number


lastly, this kind of *** is not tolerated - what happens if the source file is BIG?

"""
// TODO: could be a coroutine for instance or some iterator ~s
std::vector<std::string> loadPayloads(const std::string& path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("could not open feed file: " + path);
  }
  std::vector<std::string> payloads;
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      payloads.push_back(line);
    }
  }
  if (payloads.empty()) {
    throw std::runtime_error("feed file has no payloads: " + path);
  }
  return payloads;
}
"""

make this horror either an iterator or some coroutine if the c++ standard level allows
