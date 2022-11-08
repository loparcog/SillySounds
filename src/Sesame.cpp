/*
    Silly Sounds > Sesame
    Clock modulator for repeat, swing, and delay
    Giacomo Loparco 2022
*/

#include "plugin.hpp"

struct Sesame : Module
{

    enum ParamId
    {
        SWING_PARAM,
        DELAY_PARAM,
        REPEAT_PARAM,
        PARAMS_LEN
    };
    enum InputId
    {
        CLOCK_INPUT,
        SWINGMOD_INPUT,
        DELAYMOD_INPUT,
        REPEATMOD_INPUT,
        TRIGGER_INPUT,
        INPUTS_LEN
    };
    enum OutputId
    {
        OUT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId
    {
        LIGHT_LIGHT,
        LIGHTS_LEN
    };

    // VARIABLE DECLARATIONS

    // Tool to robustly check clock/signal rises
    rack::dsp::SchmittTrigger clockTrigger;
    rack::dsp::SchmittTrigger toggleTrigger;

    // Clock managing values
    float clkCurrent = 0;
    dsp::Timer clkTimer;
    float clkPeriod = -1;
    float clkRise = 0;

    // Input managing values
    float parSwing = 0;
    float parRepeat = 0;
    float parDelay = 0;

    // Boolean for swing beat count
    bool isSecondBeat = false;
    float modPeriod = 0;

    // Boolean for repeater mod
    bool isRepeating = false;

    // Output variable
    float outValue = 0;

    Sesame()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(SWING_PARAM, 0.f, 1.f, 0.f, "");
        configParam(DELAY_PARAM, 0.f, 1.f, 0.f, "");
        configParam(REPEAT_PARAM, 0.f, 1.f, 0.f, "");
        configInput(CLOCK_INPUT, "");
        configInput(SWINGMOD_INPUT, "");
        configInput(DELAYMOD_INPUT, "");
        configInput(REPEATMOD_INPUT, "");
        configInput(TRIGGER_INPUT, "");
        configOutput(OUT_OUTPUT, "");
    }

    void process(const ProcessArgs &args) override
    {

        /*
            GETTING THE PERIOD
            First, we can calculate the period of the input clock signal
            by how much time passes between rises
        */

        // TODO: Check if plugged in?
        // Or just generate a clock in this instead?

        // Add the sample time to the timer
        clkTimer.process(args.sampleTime);
        // Keep the current time in a variable for future calculation use
        clkCurrent = clkTimer.getTime();

        // Check if we are on a clock rise
        if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage()))
        {
            // Set the current clock period to the timer and reset it
            clkPeriod = clkCurrent;
            clkTimer.reset();

            // SWING
            // Flip the isSecondBeat flag
            isSecondBeat = !isSecondBeat;

            // REPEAT
            // Turn off the repeating flag
            isRepeating = false;
        }

        /*
            MOD THE OUTPUT
            Second, we can mod the output with a swing, repeat, or delay
            The order of priority will be swing > repeat > delay, but in essence,
            repeat will mainly be setting a flag, swing will be creating output
            w.r.t repeat, and delay will hold output in a buffer if needed
        */

        // Get the value of any knobs and mod inputs
        parSwing = params[SWING_PARAM].getValue();
        parRepeat = params[REPEAT_PARAM].getValue();
        parDelay = params[DELAY_PARAM].getValue();

        // Set the clock rise time to half of the period, reduced by how much swing is set to
        // More swing = smaller clock rise in the same period
        clkRise = (clkPeriod / 2) * (1 - parSwing);
        // Also set the modded period, which is the time needed to wait before rising w.r.t swing
        modPeriod = clkPeriod * parSwing;

        // REPEAT
        // Check if we should be repeating the signal
        if (toggleTrigger.process(inputs[TRIGGER_INPUT].getVoltage()))
        {
            // Set the flag
            isRepeating = true;
        }

        // SWING
        // Check if we should play the first beat
        if (!isSecondBeat)
        {
            // For first beat, check that we have waited for the first beat (period * swing) &&
            // we haven't played too long (start + width * (1 - swing))
            // This has been shortened to use modPeriod to shorten code and make it a bit more reasonable
            // Also check for repeating
            if (clkCurrent >= modPeriod && clkCurrent <= modPeriod + clkRise)
            {
                outValue = 10;
            }
            // Otherwise turn output off
            else
            {
                outValue = 0;
            }
        }
        // Check if we should still be playing the second beat or if it should be repeated
        else
        {
            // Base and repeating case
            if (clkCurrent <= clkRise)
            {
                outValue = 10;
            }
            else
            {
                outValue = 0;
            }
        }

        // DELAY
        //

        /*
            SEND THE OUTPUT
            Finally, we set the output values for our light and out port
        */

        outputs[OUT_OUTPUT].setVoltage(outValue);
        // Light takes a brightness from 0-1, so set it to that
        lights[LIGHT_LIGHT].setBrightness(outValue / 10);
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

        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.62, 39.0)), module, Sesame::SWING_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(32.569, 39.0)), module, Sesame::DELAY_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.32, 72.881)), module, Sesame::REPEAT_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.32, 28.825)), module, Sesame::CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.62, 51.5)), module, Sesame::SWINGMOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32.569, 51.5)), module, Sesame::DELAYMOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.62, 85.055)), module, Sesame::REPEATMOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32.569, 85.055)), module, Sesame::TRIGGER_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.32, 104.641)), module, Sesame::OUT_OUTPUT));

        addChild(createLightCentered<LargeLight<YellowLight>>(mm2px(Vec(20.62, 44.105)), module, Sesame::LIGHT_LIGHT));
    }
};

Model *modelSesame = createModel<Sesame, SesameWidget>("Sesame");