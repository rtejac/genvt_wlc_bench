#!/usr/bin/env python3
#
# INTEL CONFIDENTIAL
#
# Copyright 2021 (c) Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them  is governed by the  express license under which  they were
# provided to you ("License"). Unless the License provides otherwise, you  may
# not  use,  modify,  copy, publish,  distribute,  disclose  or transmit  this
# software or the related documents without Intel"s prior written permission.
#
# This software and the related documents are provided as is, with no  express
# or implied  warranties, other  than those  that are  expressly stated in the
# License.
#
# ----------------------------------------------------------------------------
import logging

import paho.mqtt.client as mqtt


class BrokerException(Exception):
    """
    Class for Broker exceptions
    """
    pass


class Broker(object):
    """
    Broker class, wrapper around Paho MQTT Client

    """

    def __init__(self, host, port):
        """
        Init a MQTT broker
        :param host: hostname
        :param port: port
        """
        host = host
        port = port

        try:
            self.client = mqtt.Client()
            self.client.connect(host, port)
            logging.info(f"Connected to MQTT broker: {host} on port: {port}")
        except mqtt.socket.error:
            logging.error("Check if Mosquitto server is running!")
            raise

        self.subscription_list = {}

    def subscribe(self, topic, callback):
        """
        Subscribe to a topic and register a callback
        :param topic: topic name
        :param callback method
        :return:
        """

        def _message_callback(client, userdata, message):
            callback(message.topic, message.payload.decode(encoding="utf-8"))

        try:
            self.client.subscribe(topic)
            self.client.message_callback_add(topic, _message_callback)
            self.subscription_list[topic] = callback
        except Exception as e:
            raise BrokerException(e)

    def unsubscribe(self, topic):
        """
        Unsubscribe from a given topic
        :param topic: topic name
        :return: None
        """

        try:
            self.client.unsubscribe(topic)
        except Exception as e:
            raise BrokerException(e)
        logging.info(f"Unsubscribed from topic: {topic}")

    def publish(self, topic, payload):
        """
        Publish a message on a given topic
        :param topic: topic name
        :param payload: message to be sent
        :return: None
        """
        logging.debug(f"Publishing message: {payload} on topic: {topic}")
        self.client.publish(topic, payload.encode("utf-8"))

    def loop(self, timeout=1.0):
        """
        Manual loop for MQTT network events, specify timeout (in secs) to process events
        for that much time
        :param timeout: time in seconds
        :return: None
        """
        self.client.loop(timeout=timeout)

    def start(self):
        """
        Start MQTT network loop to process events. Non blocking
        :return: None
        """
        self.client.loop_start()

    def stop(self):
        """
        Stop MQTT network loop. Then, disconnect client connection from broker
        :return: None
        """
        self.client.loop_stop()
        self.client.disconnect()
        logging.info(f"Disconnected from MQTT broker")
