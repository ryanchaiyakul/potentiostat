import matplotlib.pyplot as plt
import numpy as np

import abc

class ZModel(abc.ABC):
    """
    Abstract base class for any model of untraditional electrical components.
    Has two methods to visualize the impedances.
    """
    
    @abc.abstractmethod
    def impedance(self, omega: float) -> complex:
        pass

    def plot_impedance(self, f = None):
        if f is None:
            f = np.logspace(0, 6)

        zs = self.impedance(f * 2 * np.pi)
        
        fig, ax1 = plt.subplots()

        # Plot magnitude
        ax1.semilogx(f, np.sqrt(zs.real ** 2 + zs.imag ** 2), label = "Magnitude")
        ax1.tick_params(axis='y')
        ax1.set_xlabel("Frequency (Hz)")
        ax1.set_ylabel("Impedance (Ω)")

        # Plot phase
        ax2 = ax1.twinx()
        ax2_color = 'tab:red'
        ax2.semilogx(f, (np.arctan(zs.imag / zs.real) * 180) / np.pi, label = "Phase", color = ax2_color)
        ax2.tick_params(axis='y', labelcolor = ax2_color)
        ax2.set_ylabel('Phase (°)', color = ax2_color)

        fig.suptitle("Magnitude and Phase of CPE")
        plt.show()

    def plot_capacitance(self, f = None):
        if f is None:
            f = np.logspace(0, 6)

        zs = self.impedance(f * 2 * np.pi)
        cs = -1 / (f * 2 * np.pi * zs.imag)  # Im{Z} = -1 / wC; C = - 1 / wIm{Z}

        # Plot capacitance
        plt.semilogx(f, 20 * np.log10(cs), label = "dB")
        plt.xlabel("Frequency (Hz)")
        plt.ylabel("Capacitance (dB)")

        plt.title("Capacitance of CPE")
        plt.show()

        plt.semilogx(f, cs, label = "dB")
        plt.xlabel("Frequency (Hz)")
        plt.ylabel("Capacitance (C)")

        plt.title("Capacitance of CPE")
        plt.show()

    def plot_nyquist(self, f = None, alts = None):
        """
        Nyquist plot of impedances for 1 to 10^6 or provided frequency range. Additional impedance functions can be passed as (func, "label")
        """
        if f is None:
            f = np.logspace(0, 6)
        
        zs = self.impedance(f * 2 * np.pi)
        
        z_alts = []
        if alts is not None:
            for z_alt in alts:
                z_alts.append((z_alt[0](f * 2 * np.pi), z_alt[1]))

        # Nyquist is Re{Z} -Im{Z}
        plt.plot(zs.real, -zs.imag, label="Circuit")
        plt.scatter(zs.real, -zs.imag, alpha =0.5)

        for z, label in z_alts:
            plt.plot(z.real, -z.imag, label=label)
            plt.scatter(z.real, -z.imag, alpha =0.5)

        # Labeling
        plt.ylabel("-Z'' (Ω)")
        plt.xlabel("Z' (Ω)")
        plt.title("Nyquist Plot of CPE")
        plt.legend()
        plt.show()