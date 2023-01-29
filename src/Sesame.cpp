/*
    Silly Sounds > Sesame
    Clock modulator for burst repeating and swing
    Giacomo Loparco 2022
*/

#include "plugin.hpp"

struct Sesame : Module
{
    enum ParamId
    {
        SWING_PARAM,
        SWINGMODAMP_PARAM,
        REPEAT_PARAM,
        REPEATMODAMP_PARAM,
        SWAP_PARAM,
        PARAMS_LEN
    };
    enum InputId
    {
        CLOCK_INPUT,
        SWINGMOD_INPUT,
        TRIGGER_INPUT,
        REPEATMOD_INPUT,
        INPUTS_LEN
    };
    enum OutputId
    {
        OUT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId
    {
        SWINGLIGHT_LIGHT,
        REPEATLIGHT_LIGHT,
        LIGHTS_LEN
    };

    Sesame()
    {
        // Setting all of the knobs, inputs, and output ranges and labels
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(SWING_PARAM, 0.f, 100.f, 0.f, "Swing amount", "%");
        // Set snap so it snaps to whole numbers
        paramQuantities[SWING_PARAM]->snapEnabled = true;
        configParam(SWINGMODAMP_PARAM, -1.f, 1.f, 0.f, "Mod influence");
        configParam(REPEAT_PARAM, 1.f, 8.f, 1.f, "Repeat frequency", "x");
        paramQuantities[REPEAT_PARAM]->snapEnabled = true;
        configParam(REPEATMODAMP_PARAM, -1.f, 1.f, 0.f, "Mod influence");
        configParam(SWAP_PARAM, 0, 1, 0, "Swap main swing beat");
        configInput(CLOCK_INPUT, "Global clock");
        configInput(SWINGMOD_INPUT, "Swing amount mod");
        configInput(TRIGGER_INPUT, "Repeat trigger");
        configInput(REPEATMOD_INPUT, "Repeat frequency mod");
        configOutput(OUT_OUTPUT, "Output");
    }

    // VARIABLE DECLARATIONS

    // Tool to robustly check clock/signal rises
    rack::dsp::SchmittTrigger clockTrigger;
    rack::dsp::SchmittTrigger toggleTrigger;

    // Clock tracker and managing values
    float clkCurrent = 0;
    rack::dsp::Timer clkTimer;
    float clkPeriod = 0;

    // Parameter managing values
    float parSwing = 0;
    float parRepeat = 1;
    int buttonOn = 0;

    // Boolean for swing beat count
    bool isFirstBeat = true;
    float modPeriod = 0;

    // Variable to hold output voltage
    float outValue = 0;

    /*
        THE PROCESS
        This is the function that is run once every sample period, which I think
        by default is around 48Hz, so it is run regularly and rapidly. Anything
        happening in the module is in here
    */
    void process(const ProcessArgs &args) override
    {

        // By default, have the output as 0
        outValue = 0;

        // Make sure a clock signal is plugged in before we do any processing
        if (inputs[CLOCK_INPUT].isConnected())
        {
            /*
                GETTING THE PERIOD
                First, we will need to know the period of the input clock signal.
                We can get this by tracking the time between clock rises
                NOTE: We can provide our own, but I just decided against it for now
            */

            // Add the amount of time between functions calls, or the sample time, to the timer
            clkTimer.process(args.sampleTime);
            // Get the current time to use for processing of the current function call
            clkCurrent = clkTimer.getTime();

            // Check if we are on a clock rise (0 -> 10)
            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage()))
            {
                // Set the current clock period to the current timer time, and reset the timer
                clkPeriod = clkCurrent;
                clkTimer.reset();
                clkCurrent = 0;

                // SWING
                // Flip the isFirstBeat flag, each rise we're flipping from the first and
                // second beat
                isFirstBeat = !isFirstBeat;

                // REPEAT
                // Set the repeating frequency value to 1 and turn off the light
                parRepeat = 1;
                lights[REPEATLIGHT_LIGHT].setBrightness(0);
            }

            /*
                MOD THE OUTPUT
                Second, we can mod the output with a swing or repeat. Swing will be done by
                making the clock periods smaller, putting pairs of beats together and spacing
                each pair from one another proportionately with the current swing value. The
                repeater will act on this edited period, splitting each clock signal into n
                equally spaced clock signals. The setup is done in a way that you can do one
                or the other, or both, or neither, but then why are you even using this module
            */

            // Check if the swap button was pressed to switch the first and second beats
            if (params[SWAP_PARAM].getValue() > buttonOn)
            {
                isFirstBeat = !isFirstBeat;
            }
            // Set the button value
            buttonOn = params[SWAP_PARAM].getValue();

            // Get the value of the swing knob and add in any mod value, clamp from 0-1
            parSwing = clamp((params[SWING_PARAM].getValue() / 100) +
                                 ((inputs[SWINGMOD_INPUT].getVoltage() / 10) * params[SWINGMODAMP_PARAM].getValue()),
                             0.f, 1.f);

            // Set the modulated period, based off of how much swing there is
            // More swing = smaller period
            modPeriod = clkPeriod * (1 - parSwing);

            // REPEAT
            // Check if we should be repeating the signal by checking the trigger input
            if (toggleTrigger.process(inputs[TRIGGER_INPUT].getVoltage()))
            {
                // Get the value of the repeater knob and add in any mod value, clamp from 1-8
                parRepeat = clamp(params[REPEAT_PARAM].getValue() +
                                      ((inputs[REPEATMOD_INPUT].getVoltage() / 10) * params[REPEATMODAMP_PARAM].getValue()) * 8,
                                  1.f, 8.f);
                // Cast into a whole number
                parRepeat = floor(parRepeat);
                // Set the light on as well
                lights[REPEATLIGHT_LIGHT].setBrightness(1);
            }

            // SWING
            // Make sure we have a period to avoid constant 10V at start
            if (clkPeriod)
            {
                // Check if we are on the first beat
                if (isFirstBeat)
                {
                    // Wait until the current time reaches the start of the modded period time
                    if (clkCurrent >= clkPeriod - modPeriod)
                    {
                        /*
                            This output is set to 10 for half of the mod period, and 0 for the second
                            half, as regular clock signals would be. However, this is done through
                            some repeater math as well. Essentially, for n repeats, we are dividing the
                            period into n groups, each group having the first half high (10) and the
                            second half low (0). If there is no repeater trigger, the parRepeat value
                            is set to 1 to mimic just a single beat
                        */
                        outValue = (10 - (int(((clkCurrent - (clkPeriod - modPeriod)) * (parRepeat * 2)) / modPeriod) % 2) * 10);
                    }
                }
                // If we're on the second beat, we want to play until we reach the end of the modded
                // period time, always starting at the rise of the original second beat
                else if (clkCurrent <= modPeriod)
                {
                    // Same math, don't need to do some period check though since the clock time is
                    // lined up
                    outValue = (10 - (int((clkCurrent * (parRepeat * 2)) / modPeriod) % 2) * 10);
                }
            }
        }

        // Send the output and set the swing light based on this output
        outputs[OUT_OUTPUT].setVoltage(outValue);
        lights[SWINGLIGHT_LIGHT].setBrightness(outValue / 10);
    }
};

struct SesameWidget : ModuleWidget
{
    SesameWidget(Sesame *module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Sesame.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<VCVButton>(mm2px(Vec(21.376, 39.693)), module, Sesame::SWAP_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.104, 40.125)), module, Sesame::SWING_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(9.104, 52.76)), module, Sesame::SWINGMODAMP_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(9.104, 84.044)), module, Sesame::REPEAT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(9.104, 97.5)), module, Sesame::REPEATMODAMP_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(21.376, 14.0)), module, Sesame::CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(21.376, 53.074)), module, Sesame::SWINGMOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(21.376, 84.044)), module, Sesame::TRIGGER_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(21.376, 97.5)), module, Sesame::REPEATMOD_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(21.376, 119.5)), module, Sesame::OUT_OUTPUT));

        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(8.0, 26.601)), module, Sesame::SWINGLIGHT_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(8.0, 71.341)), module, Sesame::REPEATLIGHT_LIGHT));
    }
};

Model *modelSesame = createModel<Sesame, SesameWidget>("Sesame");