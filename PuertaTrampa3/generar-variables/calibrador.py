import os
import serial
import sys
from PyQt4 import QtGui, QtCore
from variables import variables

SERIAL_PORT = "/dev/tty.usbserial-A700dYLI"
#SERIAL_PORT = "/dev/tty.usbserial-A800eIxN"
SERIAL_PORT = ["/dev/" + o for o in os.listdir("/dev") if o.startswith("tty.usb")][0]


class TuneinoWindow(QtGui.QWidget):
    def __init__(self, serialport):
        super(TuneinoWindow, self).__init__()
        self.spinboxes = []
        self.sliders = []
        self.serialport = serialport
        self.initUI()
        
    def initUI(self):
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(100)

        grid = QtGui.QGridLayout()
        grid.setColumnMinimumWidth(1, 300)
        self.setLayout(grid)    

        for n, variable in enumerate(variables):
            name, default_value, (min_value, max_value) = variable
            label = QtGui.QLabel(name.lower().replace("_", " "))

            spinbox = QtGui.QSpinBox()
            spinbox.setRange(min_value, max_value)
            self.spinboxes.append(spinbox)

            slider = QtGui.QSlider(1)
            slider.setRange(min_value, max_value)
            self.sliders.append(slider)
            slider.setSizePolicy(QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Expanding);

            spinbox.valueChanged.connect(lambda v, index=n: self.update(index, v))
            slider.valueChanged.connect(lambda v, spinbox=spinbox: spinbox.setValue(v))
            slider.setValue(default_value)

            grid.addWidget(label, n, 0, alignment=QtCore.Qt.AlignRight)
            grid.addWidget(slider, n, 1)
            grid.addWidget(spinbox, n, 2)
        
        self.setWindowTitle('Tuneino')    
        self.show()
        
    def update(self, index, value, feedback=True):
        self.sliders[index].setValue(value)
        line = "set %d %d\n" % (index, value)
        if feedback:
            self.serialport.write(line)
    
    def read_serial(self):
        line = self.serialport.readline().strip()
        while line:
            print line
            if line.startswith("set"):
                n, x = map(int, line[4:].split(" "))
                self.update(n, x, feedback=False)
                print "setting", n, "=", x
            line = self.serialport.readline().strip()

def main():
    app = QtGui.QApplication(sys.argv)
    #serialport = open("demo.txt", "w")
    serialport = serial.Serial(SERIAL_PORT, 115200)
    print "handshake", serialport.readline().strip()
    serialport.timeout = 0
    tuneino = TuneinoWindow(serialport)
    sys.exit(app.exec_())
    serialport.close()  

if __name__ == '__main__':
    main()
