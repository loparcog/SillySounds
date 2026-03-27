/*
    Silly Sounds > Kyle
    Sidechain module w/ envelope follower
    Gillian Loparco 2026
*/

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
    std::array<float, 16> currentVoltage = {};
    // Voltage of the output signal
    std::array<float, 16> outVoltage = {};
    // Time since we hit the current input signal
    std::array<float, 16> t = {};
    // Number of 0's sequentially from input
    std::array<float, 16> n0 = {};
    // Number of input and output channels
    int channels = 0;

    void setOutputs(float out, int channel)
    {
        // Assign direct and inverse outputs
        outputs[ENV_OUTPUT].setVoltage(out, channel);
        outputs[ENVINV_OUTPUT].setVoltage(10 - out, channel);
    }

    void calcOutVoltage(float sRate, float sTime, int channel)
    {
        /*
            MODULE CALCULATIONS
        */

        // Add to the timer
        t[channel] += sTime;

        /*
            We decay the signal either exponentially if PEXP != 0,
            otherwise we decay linearly
            out - (decay * e^(exp))
        */
        outVoltage[channel] = outVoltage[channel] - ((params[PDECAY_PARAM].getValue() / sRate) *
                                   exp((params[PEXP_PARAM].getValue() * t[channel])));;

        /*
            If the original signal is greater than our output voltage,
                currVoltage > outVoltage
            Set the output to the signal voltage. Otherwise, use the
            decayed output voltage
        */
        if (currentVoltage[channel] >= outVoltage[channel])
        {
            outVoltage[channel] = currentVoltage[channel];
            // Reset the time
            t[channel] = 0.f;
        }

        // Amplify the output (maxing out at 10)
        float ampVoltage = std::min(10.f, abs(outVoltage[channel] * (1 + 9.f * params[PAMP_PARAM].getValue())));

        // Set output voltages, accounting for amplification
        setOutputs(ampVoltage, channel);
    }

    void process(const ProcessArgs &args) override
    {
        // POLYPHONY: Get the number of input channels
        channels = inputs[SIGNAL_INPUT].getChannels();

        /* INPUT */
        // Get input voltage (keep it positive)
        for (int c = 0; c < channels; c++)
        {
            currentVoltage[c] = abs(inputs[SIGNAL_INPUT].getPolyVoltage(c));

            /* OUTPUT */

            // Check if there is any input
            if (currentVoltage[c] < 0.01)
            {
                // Check if we should shut off (after no signal for 1s)
                if (n0[c] > args.sampleRate)
                {
                    setOutputs(0, c);
                }
                else
                {
                    // Iterate number of 0's
                    n0[c] += 1;

                    // Calculate output
                    calcOutVoltage(args.sampleRate, args.sampleTime, c);
                }
            }
            else
            {
                n0[c] = 0;
                // Calculate output
                calcOutVoltage(args.sampleRate, args.sampleTime, c);
            }   
        }

        // Finally set the number of output channels
        outputs[ENV_OUTPUT].setChannels(channels);
        outputs[ENVINV_OUTPUT].setChannels(channels);
    }
};


struct KyleWidget : ModuleWidget {
	KyleWidget(Kyle* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kyle.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.62, 43.975)), module, Kyle::PDECAY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.62, 58.033)), module, Kyle::PEXP_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.62, 72.09)), module, Kyle::PAMP_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 28.0)), module, Kyle::SIGNAL_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 90.0)), module, Kyle::ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 105.5)), module, Kyle::ENVINV_OUTPUT));
	}
};


Model* modelKyle = createModel<Kyle, KyleWidget>("Kyle");