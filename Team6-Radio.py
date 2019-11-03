
from datetime              import datetime 
from subprocess            import * 
from time                  import sleep, strftime
from threading             import Thread
import time
import os
import serial
import RPi.GPIO as GPIO
import pygame


GPIO.setmode(GPIO.BOARD) 
ser=serial.Serial(
    port="/dev/ttyUSB0",
    baudrate=9600,
    parity=serial.PARITY_NONE,
    bytesize=serial.EIGHTBITS,
    timeout=0.5,
    )
time.sleep(1)

files = []
file_index = 0
for filename in os.listdir("/home/pi/musicFiles"):
    if filename.endswith(".mp3"):
        files.append("/home/pi/musicFiles/"+str(filename))

track=1
def load_playlist():              
   run_cmd("mpc add http://icecast2.rte.ie/ieradio1")
   run_cmd("mpc add http://turkmedya.radyotvonline.com/turkmedya/alemfm.stream/playlist.m3u8")
   run_cmd("mpc add http://46.20.7.125/listen.pls")
   run_cmd("mpc add http://provisioning.streamtheworld.com/pls/JOY_TURKAAC.pls")
   run_cmd("mpc add http://46.20.3.204:80/")
   run_cmd("mpc add http://powerfm.listenpowerapp.com/powerfm/abr/playlist.m3u8")
   run_cmd("mpc add http://provisioning.streamtheworld.com/pls/METRO_FMAAC.pls")
   run_cmd("mpc add http://46.20.4.60/listen.pls")
   run_cmd("mpc add http://radyo.dogannet.tv/slowturk")
   run_cmd("mpc add http://provisioning.streamtheworld.com/pls/POP90AAC.pls")
   
                                            
def run_cmd(cmd): 
   p = Popen(cmd, shell=True, stdout=PIPE, stderr=STDOUT) 
   output = p.communicate()[0] 
   return output

run_cmd("mpc clear")
load_playlist()
def getTrack():      
   output = run_cmd("mpc current")
   run_cmd("mpc play" +str(track))
   station = output [6:16]                                           
   track =  output [-20:-1]
   print(station)
   print(track)
   print(output)
   
while 1: 
    if(ser.in_waiting >0):
        line = ser.readline()
        line= line.decode('utf-8')
        
        for ex in line.split(','):
           print(line)
           if ex[0:4]=='play':             
              run_cmd("mpc play "+str(track))
              output = str(run_cmd("mpc current"))
              station = str(output [9:16])
              ListData= list(station)
              print(ListData)
              for i in range(len(ListData)):
                 ser.write(ListData[i].encode())
                 
           if ex[0:4]=='stop':
              run_cmd("mpc pause")
              
           if ex[0:8]=='volumeUp':
              incVolume=ex[9:12]
              os.system("mpc volume +3")
              run_cmd("mpc volume +3")
              
           if ex[0:10]=='volumeDown':
              decVolume=ex[11:14]
              os.system("mpc volume -3")
              run_cmd("mpc volume -3")
              
           if ex[0:11]=='nextStation':
              if track==0:
                 track = track + 1
              if track>0 and track<10:
                 track=track + 1
                 print("Track number is : " + str(track))
                 run_cmd("mpc play "+str(track))
                 output = str(run_cmd("mpc current"))
                 station = str(output [9:16])
                 ListData= list(station)
                 print(ListData)
                 for i in range(len(ListData)):
                    ser.write(ListData[i].encode())
                    
           if ex[0:15]=='previousStation':
              if track>0 and track<10:
                 track=track - 1
                 print("Track number is : " + str(track))
                 run_cmd("mpc play "+str(track))
                 output = str(run_cmd("mpc current"))
                 station = str(output [9:16])
                 ListData= list(station)
                 print(ListData)
                 for i in range(len(ListData)):
                    ser.write(ListData[i].encode())

           if ex[0:7]=='mp3Play':
               pygame.mixer.init()
               pygame.mixer.music.load(files[file_index])
               pygame.mixer.music.play()
               runningFile = str(files[0][18:25])
               ListFile= list(runningFile)
               print(ListFile)
               for i in range(len(ListFile)):
                  ser.write(ListFile[i].encode())

           if ex[0:7]=='mp3Stop':
               pygame.mixer.music.load(files[file_index])
               pygame.mixer.music.pause()

           if ex[0:8]=='nextFile':               
               file_index = (file_index + 1) % len(files)
               pygame.mixer.music.load(files[file_index])
               pygame.mixer.music.play()
               runningFile = str(files[0][18:25])
               ListFile= list(runningFile)
               print(ListFile)
               for i in range(len(ListFile)):
                  ser.write(ListFile[i].encode())

           if ex[0:12]=='previousFile':
               if file_index > 0:                                    
                  file_index = (file_index - 1) % len(files)
                  pygame.mixer.music.load(files[file_index])
                  pygame.mixer.music.play()
                  runningFile = str(files[0][18:25])
                  ListFile= list(runningFile)
                  print(ListFile)
                  for i in range(len(ListFile)):                                          
                     ser.write(ListFile[i].encode())   
     
               

                    

                 
              

           
              


              

   

