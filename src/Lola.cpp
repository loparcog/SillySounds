/*
    Silly Sounds > Lola
    Live sampler for taking input and repeating it at any interval
    Giacomo Loparco 2022
*/

#include "plugin.hpp"

struct Lola : Module
{
    enum ParamId
    {
        BRECORD_PARAM,
        BPLAY_PARAM,
        BSTOP_PARAM,
        PARAMS_LEN
    };
    enum InputId
    {
        SIGNAL_INPUT,
        IRECORD_INPUT,
        IPLAY_INPUT,
        INPUTS_LEN
    };
    enum OutputId
    {
        OUT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId
    {
        LRECORD_LIGHT,
        LPLAY_LIGHT,
        LSTOP_LIGHT,
        LIGHTS_LEN
    };

    Lola()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(BRECORD_PARAM, 0.f, 1.f, 0.f, "Start/stop recording");
        configParam(BPLAY_PARAM, 0.f, 1.f, 0.f, "Start/restart playback");
        configParam(BSTOP_PARAM, 0.f, 1.f, 0.f, "Stop playback");
        configInput(SIGNAL_INPUT, "Signal input");
        configInput(IRECORD_INPUT, "Start/stop recording trigger");
        configInput(IPLAY_INPUT, "Start/restart playback trigger");
        configOutput(OUT_OUTPUT, "Output");
    }

    // Schmitt Triggers to check for rises
    rack::dsp::SchmittTrigger recTrigger;
    rack::dsp::SchmittTrigger playTrigger;

    // Assuming a sample rate of 48Khz, sample up to 4s (48000 * 4 = 192000)
    int sampleMax = 192000;
    // Track button states
    float oldBPlay = 0.f;
    float oldBRec = 0.f;
    float oldBStop = 0.f;
    // Track if we are recording or playing audio
    bool isRecording = false;
    bool isPlaying = false;
    // Vector to hold sample and index for reading
    std::vector<float> vSample;
    int i = 0;

    void
    process(const ProcessArgs &args) override
    {
        /*
            INPUT
            Check for the recording flag to start storing the subsequent
            voltages in the vector
        */

        // Check if the record button or input trigger has been activated
        if (params[BRECORD_PARAM].getValue() > oldBRec ||
            recTrigger.process(inputs[IRECORD_INPUT].getVoltage()))
        {
            // Start recording if we were not initially
            if (!isRecording)
            {
                // Flip the recording flag, empty the sample vector
                isRecording = true;
                lights[LRECORD_LIGHT].setBrightness(1);
                vSample.clear();
            }
            // Stop recording if we were
            else
            {
                // Flip the recording flag
                isRecording = false;
                lights[LRECORD_LIGHT].setBrightness(0);
            }
        }

        // Save the new recording button value for later comparison
        oldBRec = params[BRECORD_PARAM].getValue();

        // If we're recording, start storing the current signal as a sample
        if (isRecording)
        {
            // Make sure the vector is below max size (4s of samples)
            if (static_cast<int>(vSample.size()) > sampleMax)
            {
                // If the vector is full, stop recording
                isRecording = false;
                lights[LRECORD_LIGHT].setBrightness(0);
            }
            else
            {
                // Push back the current input voltage into the vector
                vSample.push_back(inputs[SIGNAL_INPUT].getVoltage());
            }
        }

        /*
            OUTPUT
            Check if we should be playing the sample (if there is a
            sample to be played), and if not, just play a passthrough
            signal
        */

        // Check if the play button or input trigger has been activated
        if (params[BPLAY_PARAM].getValue() > oldBPlay ||
            playTrigger.process(inputs[IPLAY_INPUT].getVoltage()))
        {
            // Start recording if the sample is not empty
            if (!vSample.empty())
            {
                // Flip the flag and reset the iterator
                isPlaying = true;
                lights[LPLAY_LIGHT].setBrightness(1);
                i = 0;
                // Also stop recording
                isRecording = false;
                lights[LRECORD_LIGHT].setBrightness(0);
            }
        }

        // Set the new play button value
        oldBPlay = params[BPLAY_PARAM].getValue();

        // Check if the stop button has been pressed
        if (params[BSTOP_PARAM].getValue() > oldBStop)
        {
            // Stop playback
            isPlaying = false;
            lights[LPLAY_LIGHT].setBrightness(0);
        }

        // Save the new stop button value, and set the signal accordingly
        oldBStop = params[BSTOP_PARAM].getValue();
        lights[LSTOP_LIGHT].setBrightness(params[BSTOP_PARAM].getValue());

        // Check if we should passthrough or play the sample
        if (isPlaying)
        {
            // Make sure that we're not at the end of the sample
            if (i == static_cast<int>(vSample.size()))
            {
                // If we are, stop playing
                isPlaying = false;
                lights[LPLAY_LIGHT].setBrightness(0);
                // Send passthrough instead
                outputs[OUT_OUTPUT].setVoltage(inputs[SIGNAL_INPUT].getVoltage());
            }
            // Play the sample
            else
            {
                // Send current sample voltage to output, add to iterator
                outputs[OUT_OUTPUT].setVoltage(vSample[i++]);
            }
        }
        // Passthrough otherwise
        else
        {
            outputs[OUT_OUTPUT].setVoltage(inputs[SIGNAL_INPUT].getVoltage());
        }
    }
};

struct LolaWidget : ModuleWidget
{
    LolaWidget(Lola *module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Lola.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<VCVButton>(mm2px(Vec(7.62, 52.122)), module, Lola::BRECORD_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(7.62, 73.287)), module, Lola::BPLAY_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(7.62, 87.0)), module, Lola::BSTOP_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 28.435)), module, Lola::SIGNAL_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 43.622)), module, Lola::IRECORD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 64.787)), module, Lola::IPLAY_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 102.704)), module, Lola::OUT_OUTPUT));

        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(3.62, 36.596)), module, Lola::LRECORD_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(3.62, 58.425)), module, Lola::LPLAY_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(3.62, 81.638)), module, Lola::LSTOP_LIGHT));
    }
};

Model *modelLola = createModel<Lola, LolaWidget>("Lola");