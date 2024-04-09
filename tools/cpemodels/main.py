import matplotlib.pyplot as plt
import numpy as np

import cpemodels
import config

def randles_aptamer() -> cpemodels.RandlesCicuit:
    r1 = 230
    r2 = 37785
    t1 = 2.25e-6
    p1 = 0.64632
    t2 = 3.03e-7
    p2 = 0.92733
    delta_phi = 0.001
    omega_min = 1e-4
    omega_max = 1e6

    #cpe1.generate_model('cpe1')
    #cpe2.generate_model('cpe2')

    return cpemodels.RandlesCicuit(r1, r2, t1, p1, t2, p2, delta_phi, omega_min, omega_max)

def plot_T_vs_R():
    rs = np.logspace(-6, 6, 1000)
    phiavs = np.arange(10, 80, 10)
    Ts = np.zeros((len(phiavs), len(rs)))
    for i, phi in enumerate(phiavs):
        for j, r in enumerate(rs):
            config_model = cpemodels.getModelfromR(r, phi, config.DELTAPHI, config.OMEGAMIN, config.OMEGAMAX)
            Ts[i][j] = 20 * np.log10(config_model.getT())

    for T, phiav in zip(Ts, phiavs):
        plt.semilogx(rs, T, label = "P = {:.2}".format(phiav / 90))
    plt.ylabel("T (dB)")
    plt.xlabel("Resistor (Ω)")
    plt.title("T (dB) vs Starting Resistor Value (Ω)")
    plt.legend()
    plt.show()

def main():
    #config_model = cpemodels.getModelfromR(config.R1, config.PHIAV, config.DELTAPHI, config.OMEGAMIN, config.OMEGAMAX)
    randles = randles_aptamer()
    randles.plot_impedance()
    randles.plot_nyquist()
    randles.generate_model('randles')
    #randles.plot_capacitance()
    #config_model.generate_model('config_cpe')


if __name__ == "__main__":
    main()