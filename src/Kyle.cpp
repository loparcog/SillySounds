#include "plugin.hpp"
#include <stdlib.h>

struct Kyle : Module
{
    enum ParamId
    {
        PDECAY_PARAM,
        PEXP_PARAM,
        PAMP_PARAM,
        PARAMS_LEN
    };
    enum InputId
    {
        SIGNAL_INPUT,
        INPUTS_LEN
    };
    enum OutputId
    {
        ENV_OUTPUT,
        ENVINV_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId
    {
        LIGHTS_LEN
    };

    Kyle()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(PDECAY_PARAM, 0.f, 10.f, 0.f, "Scale of decay");
        configParam(PEXP_PARAM, -10.f, 10.f, 0.f, "Exponent of decay");
        configParam(PAMP_PARAM, 0.f, 1.f, 0.f, "Amplication of output");
        configInput(SIGNAL_INPUT, "Signal");
        configOutput(ENV_OUTPUT, "Envelope");
        configOutput(ENVINV_OUTPUT, "Inverse envelope");
    }

    // Voltage of the current input signal
    float currVoltage = 0.f;
    // Voltage of the output signal
    float outVoltage = 0.f;
    float ampOut = 0.f;
    // Time since we hit the current input signal
    float t = 0.f;
    // Number of 0's sequentially from input
    float n0 = 0;

    void calcOutVoltage(float sRate, float sTime)
    {
        /*
            MODULE CALCULATIONS
        */

        // Add to the timer
        t += sTime;

        /*
            We decay the signal either exponentially if PEXP != 0,
            otherwise we decay linearly
            out - (decay * e^(exp))
        */
        outVoltage = outVoltage - ((params[PDECAY_PARAM].getValue() / sRate) *
                                   exp((params[PEXP_PARAM].getValue() * t)));

        /*
            If the original signal is greater than our output voltage,
                currVoltage > outVoltage
            Set the output to the signal voltage. Otherwise, use the
            decayed output voltage
        */
        if (currVoltage >= outVoltage)
        {
            outVoltage = currVoltage;
            // Reset the time
            t = 0.f;
        }
        outVoltage = std::max(currVoltage, outVoltage);

        // Amplify the output (maxing out at 10)
        ampOut = std::min(10.f, abs(outVoltage * (1 + 9.f * params[PAMP_PARAM].getValue())));

        // Set output voltages, accounting for amplification
        outputs[ENV_OUTPUT].setVoltage(ampOut);
        outputs[ENVINV_OUTPUT].setVoltage(10 - ampOut);
    }

    void process(const ProcessArgs &args) override
    {
        // Get input voltage (keep it positive)
        currVoltage = abs(inputs[SIGNAL_INPUT].getVoltage());
        // Check if there is any input
        if (currVoltage < 0.01)
        {
            // Check if we should shut off (after no signal for 0.5s)
            if (n0 > args.sampleRate / 2)
            {
                outVoltage = 0;
                outputs[ENV_OUTPUT].setVoltage(outVoltage);
                outputs[ENVINV_OUTPUT].setVoltage(10 - outVoltage);
            }
            else
            {
                // Iterate number of 0's
                n0 += 1;

                // Calculate output
                calcOutVoltage(args.sampleRate, args.sampleTime);
            }
        }
        else
        {
            n0 = 0;
            // Calculate output
            calcOutVoltage(args.sampleRate, args.sampleTime);
        }
    }
};

struct KyleWidget : ModuleWidget
{
    KyleWidget(Kyle *module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Kyle.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 41.64)), module, Kyle::PDECAY_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 56.399)), module, Kyle::PEXP_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 71.719)), module, Kyle::PAMP_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 27.311)), module, Kyle::SIGNAL_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 88.55)), module, Kyle::ENV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 104.227)), module, Kyle::ENVINV_OUTPUT));
    }
};

Model *modelKyle = createModel<Kyle, KyleWidget>("Kyle");