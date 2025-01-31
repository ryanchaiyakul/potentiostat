import dataclasses
import pathlib
import numpy as np

from . import zmodel, rcmodel, spice

@dataclasses.dataclass
class RandlesCicuit(zmodel.ZModel):
    """
    This class represents a Randles circuit with two CPE elements.
    """
    r1 : float
    r2 : float
    t1 :float
    p1 :float
    t2 : float
    p2 : float
    delta_phi : float
    omega_min : float
    omega_max : float

    def __post_init__(self):
        self.cpe1 = rcmodel.RCModel.getModelfromTP(self.t1, self.p1, self.delta_phi, self.omega_min, self.omega_max)
        self.cpe2 = rcmodel.RCModel.getModelfromTP(self.t2, self.p2, self.delta_phi, self.omega_min, self.omega_max)

    def impedance(self, omega: float):
        return self.r1 + 1 / (1 / self.cpe1.impedance(omega) + 1 / (self.r2 + self.cpe2.impedance(omega)))

    def plot_nyquist(self, f = None):
        super().plot_nyquist(np.logspace(np.log(self.omega_min), np.log10(self.omega_max)))


    def generate_model(self, name: str):
        self.cpe1.generate_model('cpe1')
        self.cpe2.generate_model('cpe2')

        self.sch = spice.SpiceSchematic(name)

        # Add A/B
        self.sch.add_flag('A', 0, 0)
        self.sch.add_flag('B', 512, 0)

        self.sch.add_resistor(144, -16, self.r1, 90)
        self.sch.add_resistor(304, -16, self.r2, 90)

        self.sch.add_symbol(336, 96, 'cpe1', 270)
        self.sch.add_symbol(336, 16, 'cpe2', 270)

        self.sch.add_wire(0, 0, 48, 0)
        self.sch.add_wire(128, 0, 208, 0)
        self.sch.add_wire(288, 0, 336, 0)
        self.sch.add_wire(400, 0, 512, 0)
        self.sch.add_wire(162, 0, 162, 80)
        self.sch.add_wire(162, 80, 336, 80)
        self.sch.add_wire(400, 80, 464, 80)
        self.sch.add_wire(464, 80, 464, 0)
        
        self.sch.generate_netlist()


        a = """Version 4
SymbolType CELL
LINE Normal 16 64 16 48
LINE Normal 24 16 16 16
LINE Normal 16 3 16 0
LINE Normal 16 16 16 13
LINE Normal 18 4 16 3
LINE Normal 14 6 18 4
LINE Normal 18 8 14 6
LINE Normal 14 10 18 8
LINE Normal 18 12 14 10
LINE Normal 16 13 18 12
LINE Normal 24 19 24 16
LINE Normal 24 32 24 29
LINE Normal 26 20 24 19
LINE Normal 22 22 26 20
LINE Normal 26 24 22 22
LINE Normal 22 26 26 24
LINE Normal 26 28 22 26
LINE Normal 24 29 26 28
LINE Normal 9 40 9 33
LINE Normal 9 33 9 40
LINE Normal 9 32 9 24
LINE Normal 12 31 9 32
LINE Normal 6 31 9 32
LINE Normal 6 32 9 33
LINE Normal 12 32 9 33
LINE Normal 24 48 24 41
LINE Normal 24 41 24 48
LINE Normal 24 40 24 32
LINE Normal 27 39 24 40
LINE Normal 21 39 24 40
LINE Normal 21 40 24 41
LINE Normal 27 40 24 41
LINE Normal 9 48 24 48
LINE Normal 9 16 16 16
LINE Normal 9 24 9 16
LINE Normal 9 48 9 40
TEXT 24 8 Left 1 {}
TEXT 24 55 Left 0 {}
PIN 16 0 NONE 0
PINATTR PinName A
PINATTR SpiceOrder 1
PIN 16 64 NONE 0
PINATTR PinName B
PINATTR SpiceOrder 2
""".format("RN", "APT")

        with pathlib.Path('tmp/{}.asy'.format(name)).open('w') as f:
            f.write(a)
        