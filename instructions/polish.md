

- Have a look at comments and instructions/Senior_C___Engineer_Test_Assignment.pdf and all xxx.txt files and find missing things in the current code base. Do not implement but summarize findinds in a new file: instructions/todo.txt

- make sure all requirements on instructions/Senior_C___Engineer_Test_Assignment.pdf 
- especially I want to mae sure the following is implemented by our code base
"""
Client Service in multiple variations, that subscribe to the aggregator as gRPC 
clients, receive updates and publish them into stdout. Variations are: 
o Publisher for Best Bid-Offer 
o Publisher for Volume Bands Prices for 1M/5M/10M/25M/50M+ notional 
values bands 
o Publisher for Price Bands for BBO+ 50bps/100bps/200bps/500bps/1000bps+
"""

you might want to add demo data that is more voluminous to make sur the docker test makes sense in the context of these requirements
add some unit tests about these new things if possible

document all these changes
