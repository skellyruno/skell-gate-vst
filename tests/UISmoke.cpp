#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <iostream>
#include <memory>

// Reuse the exact processor factory linked into the VST3 wrapper.
extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main() {
    juce::ScopedJuceInitialiser_GUI initialize;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    if (!processor) { std::cerr << "Failed to create plugin processor\n"; return 1; }
    processor->prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    if (!editor) { std::cerr << "Failed to create plugin editor\n"; return 2; }
    editor->setVisible(true);
    const auto directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile("build").getChildFile("ui-snapshots");
    if (!directory.createDirectory()) { std::cerr << "Failed creating snapshot directory\n"; return 3; }
    const std::array<std::pair<int, int>, 4> sizes{
        std::pair<int, int>{479, 338}, {719, 507}, {958, 676}, {1198, 845}
    };
    juce::PNGImageFormat png;
    for (const auto& [width, height] : sizes) {
        editor->setSize(width, height);
        const auto frame = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        if (!frame.isValid() || frame.getWidth() != width || frame.getHeight() != height) {
            std::cerr << "Invalid snapshot dimensions at " << width << "x" << height << "\n";
            return 4;
        }
        int green = 0;
        for (int y = 0; y < height; y += 6)
            for (int x = 0; x < width; x += 6) {
                const auto px = frame.getPixelAt(x, y);
                if (px.getGreen() > 110 && px.getGreen() > px.getRed() + 50
                    && px.getGreen() > px.getBlue() + 40) ++green;
            }
        if (green < 10) {
            std::cerr << "Blank or missing neon controls at " << width << "x" << height << "\n";
            return 5;
        }
        const auto output = directory.getChildFile(
            "Skell-Glitch-" + juce::String(width) + "x" + juce::String(height) + ".png");
        juce::FileOutputStream stream(output);
        if (stream.failedToOpen() || !png.writeImageToStream(frame, stream)) {
            std::cerr << "Failed to write screenshot: " << output.getFullPathName().toStdString() << "\n";
            return 6;
        }
        std::cout << "UI_SMOKE: " << width << "x" << height
                  << " valid, neon samples=" << green << "\n";
    }
    editor.reset();
    processor->releaseResources();
    processor.reset();
    return 0;
}
