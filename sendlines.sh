#!/usr/bin/env bash
# change these to suit
SERVER='1.2.3.4'
U='amqpuser'
P='amqppassword'

KEY="$1"
if [ -z "$KEY" ] ; then KEY="hash" ; fi

while read LINE
do
	echo -n "$LINE" | amqpsend -h "$SERVER" -u "$U" -p "$P" --persistent amq.topic "$KEY"
done

exit 0

Usage: amqpsend [options] exchange routingkey [message]

Options:
  --host/-h host         specify the host (default: "amqpbroker")
  --port/-P port         specify AMQP port (default: 5672)
  --vhost/-v vhost       specify vhost (default: "/")
  --file/-f filename     send contents of file as message
  --user/-u username     specify username (default: "guest")
  --password/-p password specify password (default: "guest")
  --persistent           mark message as persistent
  --no-persistent        mark message as not persistent

The following environment variables may also be set:
  AMQP_HOST, AMQP_PORT, AMQP_VHOST, AMQP_USER, AMQP_PASSWORD, AMQP_PERSISTENT
Acceptable values for AMQP_PERSISENT are '1' (Not Persistent) and '2' (Persistent)
