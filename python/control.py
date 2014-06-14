#!/usr/bin/python

import serial
from threading import Lock
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler
from SocketServer import ThreadingMixIn
import argparse
import threading
from time import sleep

class Control:
  def __init__(self):
    parser = argparse.ArgumentParser(description='Firestorm carrier control.')
    parser.add_argument('-s', '--serial', type=str, help='serial device to which the arduino is connected')
    self.args = parser.parse_args()    

    self.serial = serial.Serial(self.args.serial, 9600)
    sleep(4) # sleep for 4 seconds for the arduino to reboot if it still has the appropriate links.

    self.serialLock = Lock()

  def getBatteryLevel(self):
    self.serialLock.acquire()
    self.serial.write("b\n")
    line = "D"
    while line[0] == 'D':
      line = self.serial.readline()
#      print repr(line)
    
    if line[0] != 'B':
      print "ERR: recieved an unexpected response from battery request: %s" % (repr(line), )
      return None
    retval = float(line[1:]) #take a float from all except the leading 'b'
    self.serialLock.release()
    return retval

  def sendAllStop(self):
    self.serialLock.acquire()
    self.serial.write("S\n")
    #We should be about to loose power but FWIW we will return!
    self.serialLock.release()

  def main(self):
    #start the webserver then look for commands from stdin
    serverAddress = ('', 8000)

    server = CarrierControlServer(serverAddress, CarrierControlServerRequestHandler)
    # Start a thread with the server -- that thread will then start one
    # more thread for each request
    serverThread = threading.Thread(target=server.serve_forever)
    # Exit the server thread when the main thread terminates
    serverThread.daemon = True
    serverThread.start()

    while True:
      value = raw_input("Command? : ")
      if (value == "b"):
        print self.getBatteryLevel()
      elif (value == "S"):
        self.sendAllStop()
      else:
        print "huh?!"


class CarrierControlServer(ThreadingMixIn, HTTPServer):
  pass


class CarrierControlServerRequestHandler(BaseHTTPRequestHandler):
  #TODO guard /allStop behing POST instead of GET.
  def do_GET(self):
    path = self.path.rstrip('/')
    if (path == "/allStop"):
      main.sendAllStop()
      self.send_response(200)
      self.end_headers()
      self.wfile.write("Stopping.")
    elif (path == "/battery"):
      self.send_response(200)
      self.end_headers()
      self.wfile.write(main.getBatteryLevel())
    else:
      self.send_response(404, "Unknown end-point")


main = Control()
main.main()
print "done."
