#include "plugin.hpp"
#include <stdlib.h>

struct Kyle : Module
{
    enum ParamId
    {
        PTYPE_PARAM,
        PVIMP_PARAM,
        PDECAY_PARAM,
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
        configParam(PTYPE_PARAM, 0.f, 2.f, 0.f, "");
        configParam(PVIMP_PARAM, 0.f, 1.f, 0.f, "");
        configParam(PDECAY_PARAM, 0.f, 1.f, 0.f, "");
        configInput(SIGNAL_INPUT, "");
        configOutput(ENV_OUTPUT, "");
        configOutput(ENVINV_OUTPUT, "");
    }

    // Store the current input and held voltages
    float currVoltage = 0.f;
    float outVoltage = 0.f;

    // Hold the time from the last passthrough
    float t = 0.f;

    // Create functions for each decay value

    // Exponential, return function of time
    float expDecay()
    {
        // e^(t * decay) * currVoltage / 10
        return exp(t * (params[PDECAY_PARAM].getValue() * 10.f)) * (currVoltage / 7500.f);
    }

    // Linear, return constant decay
    float linDecay()
    {
        // linear, just decay
        return params[PDECAY_PARAM].getValue() / 1000.f;
    }

    // Logarithmic, return function of time
    float logDecay()
    {
        // log(t * decay) * currVoltage / 10
        return std::max(0.f, log(t * (params[PDECAY_PARAM].getValue() * 10.f)) * (currVoltage / 10.f));
    }

    void process(const ProcessArgs &args) override
    {
        // Abs the current value, make all peaks positive
        currVoltage = abs(inputs[SIGNAL_INPUT].getVoltage());
        // Set the output
        // If the signal is greater than the current output voltage
        if (currVoltage > outVoltage)
        {
            // Set the output to the signal voltage
            outVoltage = currVoltage;
            t = 0.f;
        }
        // If the current output voltage is greater than the signal
        else
        {
            // Reduce the output by a given decay type value
            switch (static_cast<int>(params[PTYPE_PARAM].getValue()))
            {
            case 0:
                outVoltage -= expDecay();
                break;
            case 1:
                outVoltage -= linDecay();
                break;
            case 2:
                outVoltage -= logDecay();
                break;
            }
            t += args.sampleTime;
        }
        // Set the output
        outputs[ENV_OUTPUT].setVoltage(outVoltage);
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

        addParam(createParam<CKSSThreeHorizontal>(mm2px(Vec(6.015, 38.593)), module, Kyle::PTYPE_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.62, 59.49)), module, Kyle::PVIMP_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(12.793, 72.119)), module, Kyle::PDECAY_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 28.711)), module, Kyle::SIGNAL_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 89.74)), module, Kyle::ENV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(12.793, 104.494)), module, Kyle::ENVINV_OUTPUT));
    }
};

Model *modelKyle = createModel<Kyle, KyleWidget>("Kyle");