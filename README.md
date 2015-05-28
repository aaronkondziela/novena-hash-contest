Novena Hash Contest Code
========================

This is code I banged out in a day or two for the Novena laptop hash
contest. The idea of the contest was to find hash collisions for an md5
hash, where at least N bits matched, with N potentially varying to adjust
difficulty.

I started with a crappy basic method, then added some inline assembly
using the POPCNT instruction to make it much faster, than progressed to
using Hashcat and AWS GPU instances, and coordinated it all via RabbitMQ
and AMQP. I could spin new instances in AWS and they'd auto-find the main
server and start working on hashes.

Of course, I did this far too late in the contest and didn't get anywhere,
and also realized that my approach was shitty and needed much refineement.

Ah well, it was fun!!
