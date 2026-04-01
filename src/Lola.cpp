/*
    Silly Sounds > Lola
    Live sampler for taking input and repeating it at any interval
    Gillian Loparco 2026
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
    std::array <float, 16> currentVoltage = {};
    // Track button states
    float oldBPlay = 0.f;
    float oldBRec = 0.f;
    float oldBStop = 0.f;
    // Track if we are recording or playing audio
    bool isRecording = false;
    bool isPlaying = false;
    // Vector to hold sample and index for reading
    // Prepare for up to 16 values per sample
    std::vector<std::array<float, 16>> sampleBuffer;
    int i = 0;

    void process(const ProcessArgs &args) override
    {
        /*
            INPUT
            Check for the recording flag to start storing the subsequent
            voltages in the vector
        */

        // POLYPHONY: Get the number of input channels
        int channels = inputs[SIGNAL_INPUT].getChannels();

        /* CHANGE RECORDING STATE */
        // Check if the record button or input trigger has been activated
        if (params[BRECORD_PARAM].getValue() > oldBRec ||
            recTrigger.process(inputs[IRECORD_INPUT].getVoltage()))
        {
            // Start recording if we were not initially
            if (!isRecording)
            {
                // Flip the recording and playing flag, empty the sample vector
                isPlaying = false;
                lights[LPLAY_LIGHT].setBrightness(0);
                isRecording = true;
                lights[LRECORD_LIGHT].setBrightness(1);
                sampleBuffer.clear();
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


        /* RECORD VALUES */
        // If we're recording, start storing the current signal as a sample
        if (isRecording)
        {
            // Make sure the vector is below max size (4s of samples)
            if (static_cast<int>(sampleBuffer.size()) > sampleMax)
            {
                // If the vector is full, stop recording
                isRecording = false;
                lights[LRECORD_LIGHT].setBrightness(0);
            }
            else
            {
                // Push back the current input voltage into the vector
                currentVoltage = {};
                // Get all samples
                for (int c = 0; c < channels; c++) 
                {
                    currentVoltage[c] = inputs[SIGNAL_INPUT].getPolyVoltage(c);
                }
                sampleBuffer.push_back(currentVoltage);
            }
        }

        /*
            OUTPUT
            Check if we should be playing the sample (if there is a
            sample to be played), and if not, just play a passthrough
            signal
        */

        /* CHANGE PLAY STATE */
        // Check if the play button or input trigger has been activated
        if (params[BPLAY_PARAM].getValue() > oldBPlay ||
            playTrigger.process(inputs[IPLAY_INPUT].getVoltage()))
        {
            // Start recording if the sample is not empty
            if (!sampleBuffer.empty())
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
            if (i == static_cast<int>(sampleBuffer.size()))
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
                // Set to all channels to ensure 0 output
                for (int c = 0; c < 16; c++)
                {
                    outputs[OUT_OUTPUT].setVoltage(sampleBuffer[i][c], c);
                }
                i++;
            }
        }
        // Passthrough otherwise
        else
        {
            // Set to all channels to ensure 0 output
            for (int c = 0; c < 16; c++)
            {
                outputs[OUT_OUTPUT].setVoltage(inputs[SIGNAL_INPUT].getPolyVoltage(c), c);
            }
        }

        // Finish by setting the number of outputs
        outputs[OUT_OUTPUT].setChannels(channels);
    }
};

struct LolaWidget : ModuleWidget
{
    LolaWidget(Lola *module)
    {
        setModule(module);
        setPanel(
            createPanel(
                asset::plugin(pluginInstance, "res/Lola.svg"),
                asset::plugin(pluginInstance, "res/Lola-dark.svg")
            )
        );

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<VCVButton>(mm2px(Vec(7.62, 53.955)), module, Lola::BRECORD_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(7.62, 75.12)), module, Lola::BPLAY_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(7.62, 88.833)), module, Lola::BSTOP_PARAM));

        addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.62, 28.0)), module, Lola::SIGNAL_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.62, 45.455)), module, Lola::IRECORD_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(7.62, 66.62)), module, Lola::IPLAY_INPUT));

        addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(7.62, 105.5)), module, Lola::OUT_OUTPUT));

        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(2.943, 38.429)), module, Lola::LRECORD_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(2.943, 60.258)), module, Lola::LPLAY_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(2.943, 83.47)), module, Lola::LSTOP_LIGHT));
    }
};

Model *modelLola = createModel<Lola, LolaWidget>("Lola");