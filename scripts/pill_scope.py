import serial
import time
import datetime
import numpy as np
#import math
import matplotlib.pyplot as plt



class PillScope(object):
    TIMEOUT_CMD = 0.1
    TIMEOUT_READ = 2.0
    VM_MAX_LEN = 500

    def __init__(self):
        if __name__ == '__main__':
            com = self.input2("Select COM port:", "COM12")
            self.ser = serial.Serial(com, 115200, timeout=0, 
                parity=serial.PARITY_NONE, 
                stopbits=serial.STOPBITS_ONE, 
                bytesize=serial.EIGHTBITS)
            self.ser.flush()
            self.receive(self.TIMEOUT_CMD)
            if not self.choose_mode():
                return
          
            self.it = 0 
            self.vm_data = []
            self.vm_data.extend(([], [], [], [], []))
            self.tm_last = datetime.datetime.now()
            self.limits()
            self.loop()
        
    def choose_mode(self):
        rx = self.send_cmd("*IDN?", self.TIMEOUT_CMD)
        print(rx)
        if (rx == ""):
            print("Device not connected!")
            return False

        rx = self.send_cmd("*RST", self.TIMEOUT_CMD)
        print(rx)
        if (rx == ""):
            print("Device not connected!")
            return False
        
        while True:
            self.mode = self.input2("\nChoose mode (SCOPE, VM, LA):", "SCOPE")
            rx = self.send_cmd("SYST:MODE " + self.mode, self.TIMEOUT_CMD)
            print(rx + "\n")
            if ("OK" in rx):
                break   
        return True

    def limits(self):
        rx = self.send_cmd("SYST:LIM?", self.TIMEOUT_CMD)
        toks = rx.split(",")
        self.lim_adc_1ch_smpl_tm = float(toks[0])
        self.lim_mem = int(toks[1])
        self.lim_la_fs = int(toks[2])
        self.lim_adcs = bool(toks[3])
        self.lim_bit8 = bool(toks[4])
        self.lim_dac = bool(toks[5])

    def loop(self):
        try:
            self.setup()
            plt.ion()
            fig=plt.figure()
            fig.canvas.set_window_title('Pill Scope - Demo')

            while True:
                if self.read():
                    self.plot()
                    self.it = self.it + 1
                    if self.mode != "VM" and self.trig_mode == "S":
                        input("Press enter for continue...")

                        if self.mode == "SCOPE":
                            self.send_cmd("SYST:MODE SCOP", self.TIMEOUT_CMD)
                        else:
                            self.send_cmd("SYST:MODE LA", self.TIMEOUT_CMD)

        except KeyboardInterrupt:
            pass
        self.ser.close()
            

    def setup(self):
        while True:
            if self.mode == "VM":
                self.avg = self.input3("Enter num of averaging samples:", "1", 1, 200)
                while True:
                    self.ch = self.input2("Enter enabled channels (XXXX => T/F):", "TTFF")
                    if len(self.ch) == 4:
                        break
                self.vcc = self.input2("Show VCC:", "True")
                self.vcc = bool(self.vcc)
            else:
                self.bits = 1
                if self.mode == "SCOPE":
                    if not self.lim_bit8:
                        self.bits = self.input2("Enter bitness (8 / 12):", "12")    
                    else:
                        self.bits = "12"       
                self.bits = int(self.bits)     
                while True:
                    self.ch = self.input2("Enter enabled channels (XXXX => T/F):", "TFFF")
                    if len(self.ch) == 4:
                        break     
                self.ch_num = self.ch.count('T')
                self.ch1 = False
                self.ch2 = False
                self.ch3 = False
                self.ch4 = False
                if self.ch[0] == 'T':
                    self.ch1 = True
                if self.ch[1] == 'T':
                    self.ch2 = True
                if self.ch[2] == 'T':
                    self.ch3 = True
                if self.ch[3] == 'T':
                    self.ch4 = True
                max_mem = self.lim_mem
                if self.bits == 12:
                    max_mem = max_mem / 2
                if self.mode == "SCOPE":
                    max_mem = max_mem / ((self.ch_num * 2) + 1)
                self.mem = self.input3("Enter memory depth", "500", 1, int(max_mem))
                self.mem = int(self.mem)
                max_fs = self.lim_la_fs
                if self.mode == "SCOPE":
                    if self.lim_adcs == 1:
                        max_fs = 1.0 / (self.lim_adc_1ch_smpl_tm * float((self.ch_num + 1)))
                    elif self.lim_adcs == 2:
                        cnt1 = int(self.ch1) + int(self.ch2) + 1
                        cnt2 = int(self.ch3) + int(self.ch4)
                        cnt_result = cnt1
                        if cnt2 > cnt1:
                            cnt_result = cnt2
                        max_fs = 1.0 / (self.lim_adc_1ch_smpl_tm * float(cnt_result))
                    else: # 4
                        cnt_result = 1
                        if self.ch1:
                            cnt_result = 2
                        max_fs = 1.0 / (self.lim_adc_1ch_smpl_tm * float(cnt_result))
                self.fs = self.input3("\nEnter sample frequency", "1000", 0, int(max_fs))            
                self.fs = int(self.fs)
                self.trig_ch = self.input3("Enter trigger channel", "1", 1, 4)
                self.trig_ch = int(self.trig_ch)
                self.trig_val = 0
                if self.mode == "SCOPE":
                    self.trig_val = self.input3("Enter trigger value in percentage", "50", 0, 100)
                self.trig_edge = self.input4("Enter trigger edge (R - Rising / F - Falling):", "R", ["R", "F"])
                self.trig_mode = self.input4("Enter trigger mode (A - Auto / N - Normal / S - Single / D - Disabled):", 
                    "N", ["A", "N", "S", "D"])
                self.trig_pre = self.input3("Enter pretrigger value in percentage", "50", 0, 100)
                self.trig_pre = int(self.trig_pre)
                 
            rx = ""   
            #self.ser.flush()
            self.receive(self.TIMEOUT_CMD)
            if self.mode == "SCOPE":

                rx = self.send_cmd('SCOP:SET {},{},{},{},{},{},{},{},{}'.format(self.bits, self.mem, self.fs, self.ch,
                    self.trig_ch, self.trig_val, self.trig_edge, self.trig_mode, self.trig_pre), self.TIMEOUT_CMD)
            elif self.mode == "LA":
                rx = self.send_cmd('LA:SET {},{},{},{},{},{}'.format(self.mem, self.fs, self.trig_ch, self.trig_edge, 
                    self.trig_mode, self.trig_pre), self.TIMEOUT_CMD)
            elif self.mode == "VM":
                rx = "OK"

            if self.mode == "VM":
                break

            print(rx)
            self.ready = False
            if "Ready" in rx:
                self.ready = True
            if "OK" in rx:
                break
            """    
            if self.mode == "SCOPE":
                rx = self.send_cmd("SCOP:SET?", self.TIMEOUT_CMD)
            elif self.mode == "LA":
                rx = self.send_cmd("LA:SET?", self.TIMEOUT_CMD)
            if "Ready" in rx:
                self.ready = True
            print("\nsettings: " + rx)
            break
            """


    def read(self):
        rx = ""
        if self.mode == "VM":
            rx = self.send_cmd("VM:READ? " + str(self.avg), self.TIMEOUT_CMD)
            toks = rx.split(",")
            if len(toks) != 5:
                print("Invalid data!")
                return False
            print(rx)
            self.vm_data[0].append(float(toks[0]))
            self.vm_data[1].append(float(toks[1]))
            self.vm_data[2].append(float(toks[2]))
            self.vm_data[3].append(float(toks[3]))
            self.vm_data[4].append(float(toks[4]))
            if len(self.vm_data[0]) > self.VM_MAX_LEN:
                del self.vm_data[0][0]
                del self.vm_data[1][0]
                del self.vm_data[2][0]
                del self.vm_data[3][0]
                del self.vm_data[4][0]
            el = datetime.datetime.now() - self.tm_last
            self.tm_last = datetime.datetime.now()
            print("\n" + str(self.it) + f"., " + str(el.total_seconds()) + " ms")
            print("---------------------")
            return True

        elif self.mode == "SCOPE":
            #if self.trig_mode != "D":
            if not self.ready:
                rx2 = self.receive(1.0)
                if ("Ready" not in rx2):
                    print(".", sep='', end='', flush=True)
                    return False
                print(rx2)
            rx = self.read_bin_data("SCOP:READ?", self.TIMEOUT_READ)
            self.ready = False
        elif self.mode == "LA":
            ex = self.read_bin_data("LA:READ?", self.TIMEOUT_READ)

        if rx == "DATA":
            pass
        elif rx == "TIMEOUT":
            print("No answer from device")
            print("---------------------")
            return False
        elif "Not ready" in rx:
            print(".", sep='', end='', flush=True)
            return False
        else:
            print(rx)
            print("---------------------")
            return False
        print("\n")

        el = datetime.datetime.now() - self.tm_last
        self.tm_last = datetime.datetime.now()

        #print(self.raw_data)

        buff = []
        if self.mode == "SCOPE":
            if self.bits == 12:
                for i in range(0, int(len(self.raw_data) / 2)):
                    j = i * 2
                    buff.append(float((self.raw_data[j + 1] << 8 | self.raw_data[j]) / 10000.0))
            else:
                for i in range(0, int(len(self.raw_data))):
                    buff.append(float((self.raw_data[i]) / 100.0))    

            buff_split = np.array_split(buff, self.ch_num)
            self.scope_data = []
            self.scope_data.extend(([], [], [], []))
            i = 0
            if self.ch1:
                self.scope_data[0] = buff_split[i]
                i = i + 1
            if self.ch2:
                self.scope_data[1] = buff_split[i]
                i = i + 1
            if self.ch3:
                self.scope_data[2] = buff_split[i]
                i = i + 1
            if self.ch4:
                self.scope_data[3] = buff_split[i]
                i = i + 1

        else: # LA
            self.la_data = []
            self.la_data.extend(([], [], [], []))
            for i in range(0, int(len(self.raw_data))):
                ch1 = self.raw_data[i] & 1 != 0
                ch2 = self.raw_data[i] & 2 != 0
                ch3 = self.raw_data[i] & 4 != 0
                ch4 = self.raw_data[i] & 8 != 0
                self.la_data[0].append(ch1)
                self.la_data[1].append(ch2)
                self.la_data[2].append(ch3)
                self.la_data[3].append(ch4)   

        print(str(self.it) + f"., {(float(len(self.raw_data)) / 1024.0):.1f} KB, " + str(el.total_seconds()) + " s")
        print("---------------------")
        return True


    def plot(self):
        ax = plt.gca()
        plt.clf()

        rngx = (self.mem / self.fs * 100)
        rngx_l = -1 * rngx * (self.trig_pre / 100.0)
        rngx_r = rngx * ((100.0 - self.trig_pre) / 100.0)

        if self.mode != "VM":
            major_ticks_x = np.linspace(rngx_l, rngx_r, 50)
            minor_ticks_x = np.linspace(rngx_l, rngx_r, 10)
            ax.set_xticks(major_ticks_x)
            ax.set_xticks(minor_ticks_x, minor=True)
            plt.xlim(rngx_l, rngx_r)
            plt.axvline(x=0, color='k', linestyle='--', linewidth=2.5, alpha=0.8)
            plt.axhline(y=3.3 / 2.0, color='k', linestyle='--', linewidth=2.5, alpha=0.8)
            plt.xlabel("Time [ms]")
        else:
            plt.xlabel("Time")
            major_ticks_x = np.arange(0, self.VM_MAX_LEN, 50)
            minor_ticks_x = np.arange(0, self.VM_MAX_LEN, 10)
            ax.set_xticks(major_ticks_x)
            ax.set_xticks(minor_ticks_x, minor=True)
            plt.xlim(0, self.VM_MAX_LEN)
        plt.ylim(0, 3.3)
        major_ticks_y = np.linspace(0, 3.3, 10)
        minor_ticks_y = np.linspace(0, 3.3, 20)
        ax.set_yticks(major_ticks_y)
        ax.set_yticks(minor_ticks_y, minor=True)
        ax.grid(which='major', alpha=0.9)
        plt.grid(True, linestyle=':')
        plt.ylabel("Voltage [V]")

        LINE_WIDTH = 2.0
        if self.mode == "SCOPE":
            t = np.linspace(rngx_l, rngx_r, self.mem)
            if self.ch1:
                plt.plot(t, self.scope_data[0], 'g', label="Channel 1", linewidth=LINE_WIDTH)
            if self.ch2:
                plt.plot(t, self.scope_data[1], 'r', label="Channel 2", linewidth=LINE_WIDTH)
            if self.ch3:
                plt.plot(t, self.scope_data[2], 'b', label="Channel 3", linewidth=LINE_WIDTH)
            if self.ch4:
                plt.plot(t, self.scope_data[3], 'm', label="Channel 4", linewidth=LINE_WIDTH)
        elif self.mode == "LA":
            t = np.arange(0, len(self.la_data[0]))
            plt.plot(t, self.la_data[0], 'g', label="Channel 1", linewidth=LINE_WIDTH)
            plt.plot(t, self.la_data[1], 'r', label="Channel 2", linewidth=LINE_WIDTH)
            plt.plot(t, self.la_data[2], 'b', label="Channel 3", linewidth=LINE_WIDTH)
            plt.plot(t, self.la_data[3], 'm', label="Channel 4", linewidth=LINE_WIDTH)
        else:
            t = np.arange(0, len(self.vm_data[0]))
            if self.ch1:
                plt.plot(t, self.vm_data[0], 'g', label="Channel 1", linewidth=LINE_WIDTH)
            if self.ch2:
                plt.plot(t, self.vm_data[1], 'r', label="Channel 2", linewidth=LINE_WIDTH)
            if self.ch3:
                plt.plot(t, self.vm_data[2], 'b', label="Channel 3", linewidth=LINE_WIDTH)
            if self.ch4:
                plt.plot(t, self.vm_data[3], 'm', label="Channel 4", linewidth=LINE_WIDTH)
            if self.vcc:
                plt.plot(t, self.vm_data[4], 'c', label="Vcc", linewidth=LINE_WIDTH)

        plt.legend(loc="lower right")
        plt.show()
        plt.pause(0.0001)  # ?    

    def receive(self, timeout):
        start = datetime.datetime.now()
        buffer_string = ""
        while True:
            buffer_string = buffer_string + (self.ser.read(self.ser.inWaiting())).decode("utf-8") 
            if (datetime.datetime.now() - start).total_seconds() > timeout:
                #print("Command timeout!")
                return ""
            if "\r\n" in buffer_string:
                return buffer_string.replace("\r\n", "").strip("\"")
                
    def input2(self, text, default):
        ret = input(text + " [" + default + "]\n")
        if (ret == ""):
            return default
        return ret

    def input3(self, text, default, lim_min, lim_max):
        while True:
            ret = input(text + " (" + str(lim_min) + " - " + str(lim_max) + "):" + " [" + default + "]\n")
            if (ret == ""):
                return default
            if int(ret) <= lim_max and int(ret) >= lim_min:
                return ret

    def input4(self, text, default, allowed):
        while True:
            ret = input(text + " [" + default + "]\n")
            if (ret == ""):
                return default
            if ret in allowed:
                return ret
                
    def send_cmd(self, cmd, timeout):
        print(cmd)
        self.ser.write(str.encode(cmd + "\r\n"))
        return self.receive(timeout)

    def read_bin_data(self, cmd, timeout):
        self.ser.write(str.encode(cmd + "\r\n"))

        state = "hash"
        num = 0
        count = 0
        start = datetime.datetime.now()
        bin_buff = b''
        rx = ""
        while True:
            if state == "hash":
                rx = self.ser.read(1).decode("utf-8")
                if len(rx) == 0:
                    pass
                elif rx == "#":
                    state = "num"
                else:
                    rx2 = self.receive(self.TIMEOUT_CMD)
                    return rx + rx2
            elif state == "num":
                rx = self.ser.read(1).decode("utf-8")
                if len(rx) == 1:
                    num = int(rx)
                    state = "count"
            elif state == "count":
                rx = self.ser.read(num).decode("utf-8")
                if len(rx) == num:
                    count = int(rx)
                    state = "data"
            elif state == "data":
                rx = self.ser.read(1)
                if len(rx) == 1:
                    bin_buff += rx
                    if len(bin_buff) == count:  
                        self.raw_data = np.frombuffer(bin_buff, dtype=np.uint8)
                        state = "crlf"
            elif state == "crlf":
                rx = self.ser.read(2).decode("utf-8")
                if "\r\n" in rx:
                    return "DATA"

            if (datetime.datetime.now() - start).total_seconds() > timeout:
                #print("Read binary data timeout!")
                print(rx)
                print(str(state))
                print(str(len(bin_buff)))
                return "TIMEOUT"


PillScope()
