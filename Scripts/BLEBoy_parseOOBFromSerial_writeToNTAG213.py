import serial
import nfc
import ndef
import struct
import binascii

oobDictionary = {}

def parseOOBData(data):
    ADDRESS_DATA_TYPE = 0x1B
    LE_ROLE_DATA_TYPE = 0x1C
    APPEARANCE_DATA_TYPE = 0x19
    LOCAL_NAME_DATA_TYPE = 0x09
    SECURITY_KEY_DATA_TYPE = 0x10
    SECURITY_KEY_SIZE = 16
    ADDRESS_SIZE = 6
    LESC_CONFIRM_TYPE = 0x22
    LESC_CONFIRM_SIZE = 16
    LESC_RANDOM_TYPE = 0x23
    LESC_RANDOM_SIZE = 16
    ADDRESS_TYPE_PUBLIC = 0x00
    ADDRESS_TYPE_RANDOM = 0x01

    #print "len" + str(len(data))
    oobDictionary = {}
    
    index = 0
    size = struct.unpack("B",data[index])[0]
    index += 1
    if struct.unpack("B",data[index])[0] == ADDRESS_DATA_TYPE:
        index += 1
        print "Address type foudn"
        addr = data[index:(index+ADDRESS_SIZE+1)]#addr + addr type(public, random)
        index += (ADDRESS_SIZE+1)
        print addr
        print binascii.hexlify(addr)
        oobDictionary[ADDRESS_DATA_TYPE] = addr
    else:
        print "Address not found. Error!"
        return {}
    size = struct.unpack("B",data[index])[0]
    index += 1
    if struct.unpack("B",data[index])[0] == SECURITY_KEY_DATA_TYPE:
        print "Found legacy TK"
        index += 1
        tk = data[index:index+SECURITY_KEY_SIZE]
        index += SECURITY_KEY_SIZE
        print tk
        print binascii.hexlify(tk)
        oobDictionary[SECURITY_KEY_DATA_TYPE] = tk
    elif struct.unpack("B",data[index])[0] == LESC_CONFIRM_TYPE:
        print "Found SC Confirm"
        index += 1
        c = data[index:index+LESC_CONFIRM_SIZE]
        index += LESC_CONFIRM_SIZE
        print c
        print binascii.hexlify(c)
        oobDictionary[LESC_CONFIRM_TYPE] = c
        #continue to pull lesc random
        size = struct.unpack("B",data[index])[0]
        index += 1
        if struct.unpack("B",data[index])[0] == LESC_RANDOM_TYPE:
            print "Found SC Random"
            index += 1
            r = data[index:index+LESC_RANDOM_SIZE]
            index += LESC_RANDOM_SIZE
            print r
            print binascii.hexlify(r)
            oobDictionary[LESC_RANDOM_TYPE] = r
        else:
            print "Missing LESC random. Error!"
    else:
        print "Unrecognized format. Expected TK or LESC Confirm. Error."
        return {}
    size = struct.unpack("B",data[index])[0]
    index += 1
    if struct.unpack("B",data[index])[0] == LOCAL_NAME_DATA_TYPE:
        index += 1
        print "Name type found"
        name = data[index:index+size]#addr + addr type(public, random)
        index += (size)
        print name
        print binascii.hexlify(name)
        oobDictionary[LOCAL_NAME_DATA_TYPE] = name
    else:
        print "Name not found. Moving on without it"

    return oobDictionary
        
def on_connect_legacy(tag):
    print "Found tag"
    print(tag)
    if tag.ndef:
        print(tag.ndef.message.pretty())
        if tag.ndef.is_writeable:
            print "Tag is writeable. Writing OOB Data"
            print "Writing Legacy OOB record"
            tag.ndef.records = [ndef.bluetooth.BluetoothLowEnergyRecord(
                (0x10,oobDictionary[0x10]),#legacy tk
                (0x09,oobDictionary[0x09]),#name
                (0x1B,oobDictionary[0x1B]))]#addr+addrtype
            print(tag.ndef.message.pretty())
            print "Tag written"
            
def on_connect_lesc(tag):
    print "Found tag"
    print(tag)
    if tag.ndef:
        print(tag.ndef.message.pretty())
        if tag.ndef.is_writeable:
            print "Tag is writeable. Writing OOB Data"
            tag.ndef.records = [ndef.bluetooth.BluetoothLowEnergyRecord(
                (0x22,oobDictionary[0x22]),#sc confirm
                (0x23,oobDictionary[0x23]),#sc random
                (0x09,oobDictionary[0x09]),#name
                (0x1B,oobDictionary[0x1B]))]#addr+addrtype
            print(tag.ndef.message.pretty())
            print "Tag written"

print "Trying to open"
ser = serial.Serial("/dev/ttyUSB1", 9600)
print "Ser created"
print ser
while not ser.is_open:
    print "Trying to open COM"
    ser.open()
print "Opened!"

lenTrigger = False
nfcTrigger = False
bodyTrigger = False
print "Please generate OOB data on the BLEBoy (it will send over the established serial connection)"
while True:
    if bodyTrigger:
        buff = ser.read(buffLen)
        print "Body:"
        print buff
        oobData = buff
        print "Done reading body!"
        bodyTrigger = False
        break
    #buff = ser.readline()
    #print "Buff: " + buff
    if "Writing NFC Payload" in buff:
        print "Reading NFC Payload"
        #read len
        buff = ser.readline()
        buffLen = int(buff.split()[1])
        print "Len is " + str(buffLen)
        bodyTrigger = True
        buff = ""

#print [i.encode("hex") for i in oobData]
oobDictionary = parseOOBData(oobData)



if oobDictionary == {}:
    "ERROR. No data to write"
else:    
    if 0x10 in oobDictionary.keys():
        print "Writing Legacy OOB record"   
        rdwr_options = {
            'on-connect': on_connect_legacy,
        }
    elif 0x22 in oobDictionary.keys() and 0x23 in oobDictionary.keys():
        print "Writing SC OOB record"
        rdwr_options = {
            'on-connect': on_connect_lesc,
        }
    else:
        print "Missing records for legacy or SC. Not re-writing tag."
        #rdwr_options = {
        #    'on-connect': on_connect_none,
        #}

    print "Attempting to open PN532 over FTDI USB"
    with nfc.ContactlessFrontend('tty:USB0') as clf:
        tag = clf.connect(rdwr=rdwr_options)
        print "Place empty NTAG213 over PN532 and wait"
        clf.close()
    print "NFC Tag written, go ahead and read tag with Android device"
    print "Going into while loop to read serial connection. Press ctrl+c to exit"
    while True:
        buff = ser.readline()
        print "Buff: " + buff
