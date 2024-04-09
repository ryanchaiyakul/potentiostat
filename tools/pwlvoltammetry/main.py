import config

def dpv(startV, endV, incrE, amplitude, pulse_width, pulse_period, quiet_time, relax_time):
    currentV = startV - amplitude
    endV -= amplitude
    prevTime = 0

    # QuietV
    yield(prevTime, currentV)
    prevTime += quiet_time

    if endV <= startV:
        incrE = -abs(incrE)
        while currentV >= endV:
            yield (prevTime, currentV)
            prevTime += pulse_period - pulse_width
            yield(prevTime - config.DCOFFSET, currentV)
            yield(prevTime, currentV + amplitude)
            prevTime += pulse_width
            yield(prevTime - config.DCOFFSET, currentV + amplitude)
            currentV += incrE
    else:
        pass

def swv(startV, endV, incrE, amplitude, frequency, quiet_time, relax_time):
    currentV = startV - amplitude
    endV -= amplitude
    prevTime = 0
    period = 1 / frequency

    # QuietV
    yield(prevTime, currentV)
    prevTime += quiet_time

    if endV <= startV:
        incrE = -abs(incrE)
        yield(prevTime- config.DCOFFSET, currentV)
        while currentV >= endV:
            yield (prevTime, currentV + amplitude)
            prevTime += period/2
            yield(prevTime - config.DCOFFSET, currentV + amplitude)
            yield(prevTime, currentV - amplitude)
            prevTime += period/2
            yield(prevTime - config.DCOFFSET, currentV - amplitude)
            currentV += incrE
    else:
        pass

def main():
    output = ""
    match config.MODE:
            case "DPV":
                for time, voltage in dpv(config.STARTV, 
                                        config.ENDV, 
                                        config.INCRE, 
                                        config.AMPLITUDE, 
                                        config.PULSEWIDTH, 
                                        config.PULSEPERIOD, 
                                        config.QUIETTIME, 
                                        config.RELAXTIME):
                    output += "{} {}\n".format(time, config.DACOFFSET - (voltage / 1000))
            case "SWV":
                for time, voltage in swv(config.STARTV, 
                                        config.ENDV, 
                                        config.INCRE, 
                                        config.AMPLITUDE, 
                                        config.FREQUENCY,
                                        config.QUIETTIME, 
                                        config.RELAXTIME):
                    output += "{} {}\n".format(time, config.DACOFFSET - (voltage / 1000))
            case _:
                raise ValueError("Unknown mode {}".format(config.MODE))
    with open(config.FILENAME, 'w') as f:
        f.write(output)

if __name__ == "__main__":
    main()