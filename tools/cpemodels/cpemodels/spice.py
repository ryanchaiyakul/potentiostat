import pathlib

class SpiceSchematic:

    def __init__(self, name):
        self.text = """Version 4\nSheet 1 1212 680\n"""
        self.name = name
        self.counts = {'r' : 0, 'c' : 0, 'X': 0}

    def add_flag(self, name: str, x: int, y: int, IO = "BiDir"):
        self.text += "FLAG {} {} {}\nIOPIN {} {} {}\n".format(x, y, name, x, y, IO)

    def add_wire(self, x1:int, y1: int, x2: int, y2: int):
        self.text += "WIRE {} {} {} {}\n".format(x1, y1, x2, y2)

    def add_symbol(self, x, y, name, rotation):
        self.counts['X'] += 1

        self.text += "SYMBOL {} {} {} R{}\n".format(name, x, y, rotation)
        self.text += "SYMATTR InstName X{}\n".format(self.counts['X'])

    def add_resistor(self, x: int, y: int, ohms: float, rotation: float = 0):
        self.counts['r'] += 1

        self.text += "SYMBOL res {} {} R{}\n".format(x, y, rotation)
        self.text += "SYMATTR InstName R{}\n".format(self.counts['r'])
        self.text += "SYMATTR Value {}\n".format(self.__float_to_str(ohms))

    def add_capacitor(self, x: int, y: int, ohms: float, rotation: float = 0):
        self.counts['c'] += 1

        self.text += "SYMBOL cap {} {} R{}\n".format(x, y, rotation)
        self.text += "SYMATTR InstName C{}\n".format(self.counts['c'])
        self.text += "SYMATTR Value {}\n".format(self.__float_to_str(ohms))

    def __float_to_str(self, val: float) -> str:
        """
        Convert a float to a LTSpice engineering string
        """
        # pico
        if val < 10**-9:
            return "{}p".format(val / 10**-12)
        # nano
        elif val < 10**-6:
            return "{}n".format(val / 10**-9)
        # micro
        elif val < 10**-3:
            return "{}u".format(val / 10**-6)
        #milli
        elif val < 1:
            return "{}m".format(val / 10**-3)
        # none
        elif val < 10**3:
            return "{}".format(val)
        # k
        elif val < 10**6:
            return "{}k".format(val / 10**3)
        # meg
        elif val < 10**9:
            return "{}meg".format(val / 10**6)
        # giga
        elif val < 10**12:
            return "{}g".format(val / 10**9)
        # terra
        else:
            return "{}t".format(val / 10**12)


    def generate_netlist(self):
        with pathlib.Path('tmp/{}.asc'.format(self.name)).open('w')  as f:
            f.write(self.text)

class SpiceSymbol:

    def __init__(self, name, sym_type = "CELL"):
        self.name = name
        self.text = "Version 4\n" + self.__get_symbol_type_str(sym_type)
        self.counts = {'p' : 0}

    def add_line(self, x1:int, y1: int, x2: int, y2: int):
        self.text += "LINE Normal {} {} {} {}\n".format(x1, y1, x2, y2)

    def add_text(self, x, y, text, align = "Left", font = 1):
        self.text += "TEXT {} {} {} {} {}\n".format(x, y, align, font, text)
    
    def add_pin(self, x, y, name, pin_type = "NONE"):
        self.counts['p'] += 1

        self.text += "PIN {} {} {} 0\n".format(x, y, pin_type)
        self.text += "PINATTR PinName {}\n".format(name)
        self.text += "PINATTR SpiceOrder {}\n".format(self.counts['p'])

    def __get_symbol_type_str(self, sym_type):
        return "SymbolType CELL\n"
    
    def generate_symbol(self):
        with pathlib.Path('tmp/{}.asy'.format(self.name)).open('w') as f:
            f.write(self.text)