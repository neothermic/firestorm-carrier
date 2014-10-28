#!/usr/bin/python

import serial
from threading import Lock
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler
from SocketServer import ThreadingMixIn
import argparse
import threading
import os
import re
import sys
import shutil
from datetime import datetime
from time import sleep

DEBUG=False
DEBUG_UTILITY_STATES=[True, True, False]
DEBUG_DRIVE_SCALE=1

class Control:
  def __init__(self):
    parser = argparse.ArgumentParser(description='Firestorm carrier control.')
    parser.add_argument('-s', '--serial', type=str, help='serial device to which the arduino is connected')
    self.args = parser.parse_args()

    self.serial = serial.Serial(self.args.serial, 9600, timeout=5)
    if not DEBUG:
      #sleep for 4 seconds for the arduino to reboot if it still has the appropriate links.
      sleep(4)
      
      #tell the arduino not to spam us with debug messages (filling up it's serial output buffer, making real messages go missing)
      self.serial.write("d\n")

      #read a line of debug output (if there is any) so that we get a complete line next time.
      try:
        self.serial.readline()
      except serial.SerialTimeoutException:
        pass
        

    self.serialLock = Lock()

  def getBatteryLevel(self):
    if DEBUG:
      return float(datetime.now().hour) + float(datetime.now().minute) / 100
    self.serialLock.acquire()
    self.serial.write("b\n")
    line = self.readNonDebugLine()

    if line == None:
      print "ERR: didn't recieve a response from battery request"
      self.serialLock.release()
      return None

    if line[0] != 'B':
      print "ERR: recieved an unexpected response from battery request: %s" % (repr(line), )
      self.serialLock.release()
      return None
    retval = float(line[1:]) #take a float from all except the leading 'B'
    self.serialLock.release()
    return retval

  def sendDriveStop(self):
    if DEBUG:
      print "DRIVE STOP CALLED"
      return
    self.serialLock.acquire()
    self.serial.write("S\n")
    self.serialLock.release()

  def sendAllStop(self):
    if DEBUG:
      print "ALL STOP CALLED"
      return
    self.serialLock.acquire()
    self.serial.write("A\n")
    #We should be about to loose power but FWIW we will return!
    self.serialLock.release()

  def getUtility(self, num):
    if DEBUG:
      return DEBUG_UTILITY_STATES[num - 1]

    self.serialLock.acquire()

    self.serial.write("u %d ?\n" % (num,))
    line = self.readNonDebugLine()

    if line == None:
      print "ERR: didn't recieve a response from utility status request"
      self.serialLock.release()
      return None

    if line[0] != 'U':
      print "ERR: recieved an unexpected response from utility status request: %s" % (repr(line), )
      self.serialLock.release()
      return None

    if line[2] == '0':
      retval = False
    elif line[2] == '1':
      retval = True
    else:
      print "ERR: recieved an unexpected response from utility status request: %s" % (repr(line), )
      retval = None

    self.serialLock.release()
    return retval

  def setUtility(self, num, state):
    if DEBUG:
      print "UTILITY %d SET TO %d" % (num, {False: 0, True: 1}[state])
      DEBUG_UTILITY_STATES[num - 1] = state
      return

    self.serialLock.acquire()
    self.serial.write("u %d %d\n" % (num, {False: 0, True: 1}[state]))
    self.serialLock.release()

    #TODO read the status as an acknowledgement
    return None

  def getDriveScale(self):
    if DEBUG:
      return DEBUG_DRIVE_SCALE

    self.serialLock.acquire()

    self.serial.write("v s ?\n")
    line = self.readNonDebugLine()

    if line == None:
      print "ERR: didn't recieve a response from drive scale request"
      self.serialLock.release()
      return None

    if line[0:4] != 'v s ':
      print "ERR: recieved an unexpected response from drive scale request: %s" % (repr(line), )
      self.serialLock.release()
      return None

    try:
      retval = int(line[4:])
    except ValueError:
      print "ERR: recieved an unexpected response from drive scale request: %s" % (repr(line), )
      retval = None

    self.serialLock.release()
    return retval

  def setDriveScale(self, newScale):
    if DEBUG:
      print "DRIVE_SCALE SET TO %s" % newScale
      global DEBUG_DRIVE_SCALE
      DEBUG_DRIVE_SCALE = newScale
      return

    self.serialLock.acquire()
    self.serial.write("v s %s\n" % (newScale))
    self.serialLock.release()

    #TODO read the status as an acknowledgement
    return None

  def monitorDebug(self):
    if DEBUG:
      try:
        while True:
          print "Yo Dawg!"
          sleep(1)
      except KeyboardInterruptException:
        return # break out of the infinite loop
        
    self.serialLock.acquire()
    self.serial.write("D\n")
    try: 
      while True:
        try:
          print self.serial.readline()
        except serial.SerialTimeoutException as e:
          print e
    except KeyboardInterruptException:
      pass # break out of the infinite loop

    self.serial.write("d\n")
    self.serialLock.release()

  def readNonDebugLine(self):
    """Read a line from self.serial, ignoring all of the debug lines. 
       You must have the self.serialLock *before* calling this function

       Returns None if we couldn't read a non-debug line in a reasonable amount of time.
    """
    line = "D"
    linesRead = 0
    try:
      while line == None or line == "" or line[0] == 'D':
        linesRead += 1
        if linesRead == 100:
          return None

        try:
          line = self.serial.readline()
          sys.stdout.write(".")
          sys.stdout.flush()
        except serial.SerialTimeoutException:
          sys.stdout.write("T")
          sys.stdout.flush()
        #print repr(line)

      return line
    except KeyboardInterrupt:
      return None

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
      if len(value) < 1:
        self.printHelp()
      elif (value == "b"):
        print self.getBatteryLevel()
      elif (value == "S"):
        self.sendAllStop()
      elif (value == "D"):
        self.monitorDebug()
      elif (value[0] == 'u'):
        print self.getUtility(int(value[2]))
      elif (value[0] == 'U'):
        self.setUtility(int(value[2]), value[4] == "1")
      elif (value[0] == 'v'):
        if (value[2] == 's'):
          if (value[4] == '?'):
            print self.getDriveScale()
          else:
            self.setDriveScale(value[4])
        else:
          print "'v' must be folowed by 's' currently."
          self.printHelp()
      else:
        print "huh?!"
        self.printHelp()

  def printHelp(self):
    print """Enter one of the following commands:
 b     - show the current battery level
 S     - stop everything, including power to the control hardware
 u 3   - get the state of utility 3.
 U 3 1 - set the state of utility 3 to "on".
 U 3 0 - set the state of utility 3 to "off".
 v s 5 - scale the drive speed to half of the maximum power.
 v s F - scale the drive speed to full power.
 D     - monitor debug messages from the arduino. ctrl-C to stop.
"""

class CarrierControlServer(ThreadingMixIn, HTTPServer):
  pass


class CarrierControlServerRequestHandler(BaseHTTPRequestHandler):
  #TODO guard /allStop behind POST instead of GET.
  def do_GET(self):
    path = self.path.rstrip('/')
    if (path == "/" or path == "/index.html"):
      self.sendFile("index.html", "text/html")
    elif (path == "/jquery.js"):
      self.sendFile("jquery.js", "text/javascript")
    elif (path == "/bootstrap.min.js"):
      self.sendFile("bootstrap.min.js", "text/javascript")
    elif (path == "/bootstrap.min.css"):
      self.sendFile("bootstrap.min.css", "text/css")
    elif (path == "/bootstrap-slider.min.js"):
      self.sendFile("bootstrap-slider.min.js", "text/javascript")
    elif (path == "/bootstrap-slider.min.css"):
      self.sendFile("bootstrap-slider.min.css", "text/css")
    elif (path == "/driveStop"):
      main.sendDriveStop()
      self.send_response(200)
      self.end_headers()
      self.wfile.write("Drive Stopping.")
    elif (path == "/allStop"):
      main.sendAllStop()
      self.send_response(200)
      self.end_headers()
      self.wfile.write("Stopping.")
    elif (path == "/battery"):
      self.send_response(200)
      self.end_headers()
      self.wfile.write(main.getBatteryLevel())
    elif (path.startswith("/utility/")):
      subpath = path[9:]
      m = re.match("^([0-9])+$", subpath)
      if m:
        #This is a query
        self.send_response(200)
        self.end_headers()
        self.wfile.write(repr(main.getUtility(int(m.group(1)))))
      else:
        m = re.match("^([0-9])+/on$", subpath)
        if m:
          #This is a set.
          #TODO guard this behind POST and remove the "/on"?
          self.send_response(200)
          self.end_headers()
          self.wfile.write(main.setUtility(int(m.group(1)), True))
        else:
          m = re.match("^([0-9])+/off$", subpath)
          if m:
            #This is a set.
            #TODO guard this behind POST and remove the "/off"?
            self.send_response(200)
            self.end_headers()
            self.wfile.write(main.setUtility(int(m.group(1)), False))
          else:
            self.send_response(404, "Unknown end-point")
    elif (path == "/driveScale"):
      #This is a query
      self.send_response(200)
      self.end_headers()
      self.wfile.write(main.getDriveScale())
    elif (path.startswith("/driveScale/set/")):
      #This is a set
      #TODO guard this behind POST and move the value out of the url?
      subpath = path[16:]
      self.send_response(200)
      self.end_headers()
      self.wfile.write(main.setDriveScale(subpath))
    else:
      self.send_response(404, "Unknown end-point")

  def sendFile(self, name, ctype="text/plain"):
    self.send_response(200)
    self.send_header("Content-type", ctype)
    self.end_headers()
    src = open(os.path.join(os.path.dirname(__file__), name), "r")
    shutil.copyfileobj(src, self.wfile)
    src.close()

main = Control()
main.main()
print "done."
