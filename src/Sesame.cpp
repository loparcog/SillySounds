/*
    Silly Sounds > Sesame
    Clock modulator for repeat and swing
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
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(SWING_PARAM, 0.f, 1.f, 0.f, "Swing amount", "%");
        // Set snap so it snaps to whole numbers
        // paramQuantities[SWING_PARAM]->snapEnabled = true;
        configParam(SWINGMODAMP_PARAM, -1.f, 1.f, 0.f, "Mod influence");
        configParam(REPEAT_PARAM, 1.f, 8.f, 1.f, "Repeat frequency", "x");
        paramQuantities[REPEAT_PARAM]->snapEnabled = true;
        configParam(REPEATMODAMP_PARAM, -1.f, 1.f, 0.f, "Mod influence");
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

    // Clock managing values
    float clkCurrent = 0;
    rack::dsp::Timer clkTimer;
    float clkPeriod = -1;

    // Input managing values
    float parSwing = 0;
    float parRepeat = 1;

    // Boolean for swing beat count
    bool isFirstBeat = true;
    float modPeriod = 0;

    // Output variable
    float outValue = 0;

    void process(const ProcessArgs &args) override
    {
        /*
            GETTING THE PERIOD
            First, we can calculate the period of the input clock signal
            by how much time passes between rises
        */

        // TODO: Check if plugged in?
        // Or just generate a clock in this instead?
        // Decisions, decisions

        // Add the sample time to the timers
        clkTimer.process(args.sampleTime);
        // Keep the current time in a variable for future calculation use
        clkCurrent = clkTimer.getTime();

        // Check if we are on a clock rise
        if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage()))
        {
            // Set the current clock period to the timer and reset it
            clkPeriod = clkCurrent;
            clkTimer.reset();
            clkCurrent = 0;

            // SWING
            // Flip the isFirstBeat flag
            isFirstBeat = !isFirstBeat;

            // REPEAT
            // Set the repeating value to 1 and turn off the light
            parRepeat = 1;
            lights[REPEATLIGHT_LIGHT].setBrightness(0);
        }

        /*
            MOD THE OUTPUT
            Second, we can mod the output with a swing, repeat, or delay
            The order of priority will be swing = repeat > delay, but in essence,
            repeat and swing will generate the clock signals and delay will
            hold any output in a buffer until it should be played
        */

        // Get the value of any knobs and mod inputs
        parSwing = params[SWING_PARAM].getValue();

        // Set the modulated period, based off of how much swing there is
        // More swing = smaller period
        modPeriod = clkPeriod * (1 - parSwing);

        // REPEAT
        // Check if we should be repeating the signal
        if (toggleTrigger.process(inputs[TRIGGER_INPUT].getVoltage()))
        {
            // Set repeater to the knob value
            parRepeat = params[REPEAT_PARAM].getValue();
            // Set the light on as well
            lights[REPEATLIGHT_LIGHT].setBrightness(1);
        }

        outValue = 0;

        // SWING
        // Check if we should play the first beat (wait for modded period)
        if (isFirstBeat)
        {
            if (clkCurrent >= clkPeriod - modPeriod)
            {
                // Send the outvalue based on the repeater content
                // This sends 10 unless
                outValue = (10 - (int(((clkCurrent - (clkPeriod - modPeriod)) * (parRepeat * 2)) / modPeriod) % 2) * 10);
            }
        }
        // Check how we should play the second beat
        else if (clkCurrent <= modPeriod)
        {
            outValue = (10 - (int((clkCurrent * (parRepeat * 2)) / modPeriod) % 2) * 10);
        }

        lights[REPEATLIGHT_LIGHT].setBrightness(isFirstBeat);

        // Send the output and set the swing light
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

        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.096, 43.419)), module, Sesame::SWING_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8.096, 56.919)), module, Sesame::SWINGMODAMP_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.096, 73.735)), module, Sesame::REPEAT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8.096, 87.235)), module, Sesame::REPEATMODAMP_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 28.684)), module, Sesame::CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.384, 56.919)), module, Sesame::SWINGMOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.384, 73.735)), module, Sesame::TRIGGER_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.384, 87.235)), module, Sesame::REPEATMOD_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24, 102.704)), module, Sesame::OUT_OUTPUT));

        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 49.52)), module, Sesame::SWINGLIGHT_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 80.485)), module, Sesame::REPEATLIGHT_LIGHT));
    }
};

Model *modelSesame = createModel<Sesame, SesameWidget>("Sesame");