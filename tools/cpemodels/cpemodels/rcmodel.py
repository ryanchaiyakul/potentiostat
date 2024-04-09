import dataclasses
import typing
import quantiphy
import numpy as np

from . import zmodel, spice

@dataclasses.dataclass
class RCModel(zmodel.ZModel):
    """
    An RC model of a CPE.
    """
    rs: typing.List[float]
    cs: typing.List[float]
    rp: float
    cp: float
    alpha: float
    phi_av: float
    delta_phi: float
    omega_min: int
    omega_max: int

    @staticmethod
    def getModelfromTP(T, P, delta_phi, omega_min, omega_max):
        """
        Create RC model of a CPE for a specific T and P pairing to match EIS data.

        Leverages the fact that the 20log(T) = a*log(R)+b
        """
        phi_av = P * 90

        # Choose simple numbers for log(100) = 2 log(1000) = 3
        sample_1 =RCModel.getModelfromR(100, phi_av, delta_phi, omega_min, omega_max)
        sample_2 = RCModel.getModelfromR(1000, phi_av, delta_phi, omega_min, omega_max)

        # calculate a and b
        a = (20 * np.log10(sample_2.getT()) - 20 * np.log10(sample_1.getT()))
        b = 20 * np.log10(sample_1.getT()) - a * 2

        # solve for log(R) in terms of 20log(T)

        r1 = 10 **((20 * np.log10(T) - b) / a)

        return RCModel.getModelfromR(r1, phi_av, delta_phi, omega_min, omega_max)

    @staticmethod
    def getModelfromR(r1, phi_av, delta_phi, omega_min, omega_max):
        """
        Create a RC model of a CPE following the algorithm in the paper linked below.

        https://onlinelibrary.wiley.com/doi/full/10.1002/cta.785
        """
        c1 = (1 / omega_min) / r1

        # get ab
        ab = 0.24 / (1 + delta_phi) # Only valid for 30 <= config.PHIAV <= 60
        a = np.e ** (phi_av * np.log(ab) / 90)
        b = ab / a

        # Find # of RC segments
        m = int(np.ceil(1 - np.log(omega_max / omega_min) / np.log(ab)))
        
        # RC Chain 
        rs = np.array([r1 * a ** (i - 1) for i in range(1, m + 1)])
        cs = np.array([c1 * b ** (i - 1) for i in range(1, m + 1)])

        rp = r1 * (1 - a) / a
        cp = c1 * b ** m / (1 - b)

        return RCModel(rs, cs, rp, cp, phi_av / -90, phi_av, delta_phi, omega_min, omega_max)

    def __post_init__(self):
        omega_av = np.sqrt(self.omega_max * self.omega_min)
        z_omega_av = self.impedance(omega_av)

        # FIXME: phi does not work
        # Calculate phi for phi/alpha representation
        self.phi = z_omega_av / omega_av ** self.alpha
        
        # Calculate T and P for lab's model
        self.P = -self.alpha
        self.T = -1j * ((1 / z_omega_av) ** (1 / self.P)) / omega_av

    def getT(self):
        return np.sqrt(self.T.imag ** 2 + self.T.real ** 2)

    def __str__(self):
        # Parameters
        ret = "CPE Parameters\n"
        #ret += "ψ: {:.4}, α: {:.3}\n".format(np.sqrt(self.phi.real ** 2 + self.phi.imag ** 2), self.alpha)
        ret += "T: {:.4}, P: {:.3}\n\n".format(self.getT(), self.P)
        
        # Configuration
        ret += "RC Model\n"
        ret += "Defined for {}Hz to {}Hz\n\n".format(quantiphy.Quantity(self.omega_min), quantiphy.Quantity(self.omega_max))
        ret += "Start Resistor: {}Ω, End Capacitor: {}C\n".format(quantiphy.Quantity(self.rp), quantiphy.Quantity(self.cp))
        for r, c, i in zip(self.rs, self.cs, range(1, len(self.rs) + 1)):
            ret += "{}. Resistor: {}Ω, Capacitor: {}C\n".format(i, quantiphy.Quantity(r), quantiphy.Quantity(c))
        return ret[:-1]

    def impedance(self, omega: float) -> complex:
        """
        Actual circuit impedance for the provided omega
        """
        # Start with correcting R and C
        admittance_sum = (1 / self.rp) + (1j * omega * self.cp)
        
        # Series RC segments Z = r + 1/jwC
        for r, c in zip(self.rs, self.cs):
            admittance_sum += 1 / (r + 1 / (1j * omega * c))

        return 1 / admittance_sum
    
    def ideal_impedance(self, omega: float) -> complex:
        """
        Ideal CPE impedance for the provided omega
        """
        return 1 / (1j * omega * self.T) ** self.P
    
    def plot_impedance(self, f=None):
        if f is None:
            f = np.logspace(np.log10(self.omega_min), np.log10(self.omega_max))
        super().plot_impedance(f)
    
    def plot_nyquist(self, f=None):
        if f is None:
            f = np.logspace(np.log10(self.omega_min), np.log10(self.omega_max))
        super().plot_nyquist(f, [(self.ideal_impedance, "T and P")])
        
    def generate_model(self, name: str):
        self.sch = spice.SpiceSchematic(name)
        spacing = 64
        
        x = 0

        # Add A/B
        self.sch.add_flag('A', x, 0)
        self.sch.add_flag('B', x, 176)
        x += spacing

        # Add rp
        self.sch.add_wire(x + 16, 48, x + 16, 0)
        self.sch.add_resistor(x, 32, self.rp)  # Centered
        self.sch.add_wire(x + 16, 128, x + 16, 176)
        x += spacing

        for r, c in zip(self.rs, self.cs):
            self.sch.add_capacitor(x, 0, c)
            self.sch.add_wire(x + 16, 96, x + 16, 64) # connecting wire
            self.sch.add_resistor(x, 80, r)
            x += spacing

        # add cp
        self.sch.add_wire(x + 16, 48, x    + 16, 0)
        self.sch.add_capacitor(x, 48, self.cp)  # Centered
        self.sch.add_wire(x + 16, 112, x + 16, 176)

        # add long wires
        self.sch.add_wire(0, 0, x + 16, 0)
        self.sch.add_wire(0, 176, x + 16, 176)
        
        self.sch.generate_netlist()

        self.symbol = spice.SpiceSymbol(name)


        # Create wonky capacitor symbol

        self.symbol.add_line(16, 36, 16, 64)
        self.symbol.add_line(16, 28, 16, 0)
        self.symbol.add_line(0, 24, 16, 28)
        self.symbol.add_line(32, 24, 16, 28)
        self.symbol.add_line(32, 32, 16, 36)
        self.symbol.add_line(16, 36, 0, 32)

        #TODO: Add working instname

        self.symbol.add_text(24, 8, "CPE")
        self.symbol.add_text(24, 52, "T={:.3}".format(self.getT()), font = 0.5)
        self.symbol.add_text(24, 60, "P={:.3}".format(self.P), font = 0.5)

        # Add ports

        self.symbol.add_pin(16, 0, 'A')
        self.symbol.add_pin(16, 64, 'B')

        
        self.symbol.generate_symbol()
