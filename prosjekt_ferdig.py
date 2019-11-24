import smbus
import time
from datetime import datetime
import serial
import RPi.GPIO as GPIO
import socket
from flask import Flask, render_template, Response
from waitress import serve
import cv2
import socket 
import io
import threading
import numpy as np

GPIO.setmode(GPIO.BCM)
GPIO.setup(17, GPIO.OUT)                #deklarering av ledlamper som viser systemets status
GPIO.setup(22, GPIO.OUT)
GPIO.setup(27, GPIO.OUT)


UDP_IP = "0.0.0.0"                      #udp-ip & port for aa motta udpmeldinger fra csharp
UDP_PORT = 9100

sock = socket.socket(socket.AF_INET,    # Internet protocol
                     socket.SOCK_DGRAM) # User Datagram (UDP)
sock.bind((UDP_IP, UDP_PORT))

time.sleep(0.1)

UDP2_IP = "10.0.0.61"                   #udp-ip & port for aa sende udp til node red
UDP2_PORT = 9200

sock2 = socket.socket(socket.AF_INET,    # Internet protocol
                     socket.SOCK_DGRAM)  # User Datagram (UDP)

time.sleep(0.1)

bus = smbus.SMBus(1)                    #deklarerer sm-bussen og adresser
address = 0x52
bus.write_byte_data(address, 0x40, 0x00)
bus.write_byte_data(address, 0xF0, 0x55)
bus.write_byte_data(address, 0xFB, 0x00)


port = "/dev/ttyACM0"                   #denne porten sender serielldata til mikrocontrolleren (mc)
SerialIOmbed = serial.Serial(port,9600) #setup serieport og baudrate
SerialIOmbed.flushInput()
SerialIOmbed.flushOutput()              #fjerne gamle inn- og -output

portbt = "/dev/ttyACM1"                 #denne porten mottar data fra ble-dongelen
Serialbt = serial.Serial(portbt,115200) 
Serialbt.flushInput()                   #fjerne gamle inn- og -output
Serialbt.flushOutput()

 
digibryter = 0                          #global verdi som holder verdien for systemets status

def cskarp():                                               #funksjon som mottar data fra c#
    global digibryter
    while True:
        data, addr = sock.recvfrom(1280)                    #forste del av funksjonen setter systemets status i 'digibryter'
        verdi=data.split(",")                               #splitter csvstring
        if((int(verdi[4])== 1) and (int(verdi[5])== 0)):    #ser paa de to siste tegnene som 
            digibryter = 1                                  #bestemmer digibryters verdi
            GPIO.output(27, GPIO.LOW)                       #led viser systemstatus
            GPIO.output(22, GPIO.HIGH)
        if((int(verdi[4])== 0) and (int(verdi[5])== 0)):
            digibryter = 0
            GPIO.output(27, GPIO.LOW)
            GPIO.output(22, GPIO.LOW)
        if((int(verdi[4])== 0) and (int(verdi[5])== 1)):
            digibryter = 2
            GPIO.output(27, GPIO.HIGH)
            GPIO.output(22, GPIO.LOW)
        if((int(verdi[4])== 1) and (int(verdi[5])== 1)):
            digibryter = 3
            GPIO.output(27, GPIO.HIGH)
            GPIO.output(22, GPIO.HIGH)
     
        print "digitbryter:", digibryter                    #print for debug
                                                            #denne delen skriver til mc naar c#styrer
        if(digibryter == 1):                                #hvis systemet er i status '1' sjekker vi data fra c# og 
            if(int(verdi[0]) == 1):                         #skriver til mc
                SerialIOmbed.write("4\n")                   #naar digibryter er '1' er det c# som styrer kameraet
            elif(int(verdi[1]) == 1):
                SerialIOmbed.write("3\n")
            elif(int(verdi[2]) == 1):
                SerialIOmbed.write("1\n")
            elif(int(verdi[3]) == 1):
                SerialIOmbed.write("2\n")
            else:
                SerialIOmbed.write("0\n")
            

def stikke():                                               #funksjon for aa bruke joystick til aa styre kamera
    global digibryter                                       
    while True:
        if(digibryter == 0):                                #gjoer kun noe naar digibryter == 0
            bus.write_byte(address, 0x00)
            time.sleep(0.1)
            data0 = bus.read_byte(address)                  #leser businngangen paa adresse 0x52
            data1 = bus.read_byte(address)            
            data = [data0, data1]                           #putter data i en liste
            joy_x = data[0]                                 #skiller data mtp forkjellig aksedata
            joy_y = data[1] 
            #logikk for aa avgjore hva python skal sende til mc:              
            if((joy_x <= 147 and joy_x >= 135) and (joy_y <= 143 and joy_y >= 131)): #her staar stikken i 'midten'
                SerialIOmbed.write("0\n")                   
            if(joy_x < 135 and (joy_y < 143 and joy_y > 131)):      #venstre i x-akse og null i y-akse 
                SerialIOmbed.write("1\n")                   
            if(joy_x > 147 and (joy_y < 143 and joy_y > 131)):      #hoyre i x-akse og null i y-akse
                SerialIOmbed.write("2\n")
            if(joy_y < 131 and (joy_x <= 147 and joy_x >= 135)):    #venstre i y-akse og null i x-akse
                SerialIOmbed.write("3\n")
            if(joy_y > 143 and (joy_x <= 147 and joy_x >= 135)):    #hoyre i y-akse og null i x-akse
                SerialIOmbed.write("4\n")
            if(joy_y < 131 and joy_x < 135):                        #tredje kvadrant, sor-vest
                SerialIOmbed.write("5\n")
            if(joy_y < 131 and joy_x > 147):                        #fjerde kvadrant, sor-ost
                SerialIOmbed.write("6\n")
            if(joy_x < 135 and joy_y > 143):                        #andre kvadrant, nord-vest 
                SerialIOmbed.write("7\n")
            if(joy_x > 147 and joy_y > 143):                        #forste kvadrant, nord-ost
                SerialIOmbed.write("8\n")


def bt():                                   #funksjonen for aa styre kamera ned blaatanndongel
    while True:
        global digibryter
        
        if(digibryter == 2):
            b1=[0,0,0,0]                    #deklarerer en liste for aa ta imot data fra blaatann serielt
            for x in range(0,4):            # for hvert trykk i appen kommer det tre symboler aa ta imot        
                b1[x]=Serialbt.read()       #leser inn ila 3 iterasjoner over usb-port 
            b=b1[1]                             #lagrer kun data vi er interresert i
            print "b0:",b1[0]                   #print for debug
            print "digitbryter:", digibryter    #print for debug
            if(b == "u"):                       #tester blatanndata og sender kommando videre til mc 
                SerialIOmbed.write("4\n")
            elif(b == "d"):
                SerialIOmbed.write("3\n")
            elif(b == "l"):
                SerialIOmbed.write("1\n")
            elif(b == "r"):
                SerialIOmbed.write("2\n")
            elif(b == "1"):
                SerialIOmbed.write("0\n")
            else:
                SerialIOmbed.write("0\n")

    
thread1 = threading.Thread(target=cskarp)       #deklarerer og starter funksjoner som skal kjores som traader
thread1.start()
thread2 = threading.Thread(target=stikke)
thread2.start()
thread3 = threading.Thread(target=bt)
thread3.start()

app = Flask(__name__)                           #deklarering av flask-
@app.route('/')

def index(): 
   """Video streaming .""" 
   return render_template('index.html')

def gen():
    global digibryter                           #endringer i funksjonen endrer variabelen globalt
    capture = cv2.VideoCapture(0)
    fgbg = cv2.createBackgroundSubtractorMOG2(50,200,True) #funksjon som skiller forgrunn fra bakgrunn
    frameCount = 0
    
    """Video streaming generator function."""
    while True:
        rval,frame = capture.read()             #her hentes neste bilde fra kamera, rval er return fra funksjon
                                                #som enten er 'true' eller 'false'
        if not rval:                            #hvis return ikke 'true' brytes while da den ikke har noe 
            break                               #bilde fra kamera

        frameCount += 1                         #inkrementerer bildetelleren
        resizedFrame = cv2.resize(frame,(0,0),fx=0.50,fy=0.50) #endrer stoerrelsen paa bildet

        
        fgmask = fgbg.apply(resizedFrame)       #her hentes 'foreground mask'
                                                
        count = np.count_nonzero(fgmask)        #teller piksler i 'foreground mask', som er antall piksler som 
        if (frameCount > 1 and count > 1500):   #er forskjellig fra forrige bilde, som naa er 'bakgrunn'
            GPIO.output(17, GPIO.HIGH)          #hvis det er fler enn 1500 pikselforandringer settes en led hoy
            
            if(digibryter == 3):                #naar c# har satt systemet til  aa vaere i tilstand '3' 
                naa = datetime.now()            #blir bevegelser rapportert og udp-melding sendt til node-red
                melding = "Alarm! Bevegelse! Tidspunkt: {}".format(naa)
                sock2.sendto(melding, (UDP2_IP, UDP2_PORT))
                digibryter = 0
                print "digitbryter:", digibryter #debugprint
                print "Sendt Message: "+melding  #

            
            time.sleep(1)                        #venter ett sekund
            GPIO.output(17, GPIO.LOW)            #setter ledden lav
                                   
        cv2.imwrite('pic.jpg', frame)            #skriver et bilde som jpeg-fil
        
        yield (b'--frame\r\n'                    #her defineres hva flask skal streame
               b'Content-Type: image/jpeg\r\n\r\n' + open('pic.jpg', 'rb').read() + b'\r\n')


@app.route('/video_feed')                        #ruta til streamen

def video_feed():                                #funksjonen til videofeeden
   return Response(gen(),                        #feeder returneringen av gen-funksjonen 
                   mimetype='multipart/x-mixed-replace; boundary=frame')
app.run(host='10.0.0.61',port=5000, threaded=True)      #deklarer host-ip og port, prosessen skal ogsaa vaere traadet
