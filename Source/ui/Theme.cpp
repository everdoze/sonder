#include "Theme.h"
#include "SonderLookAndFeel.h"

namespace sonder::ui
{

namespace
{
    constexpr juce::uint32 kDefaultAccent = 0xff2fb8c8;

    Settings loadSettings()
    {
        Settings settings;

        if (const auto xml = juce::parseXML (Settings::getFile()))
        {
            settings.theme = xml->getIntAttribute ("theme", settings.theme);
            settings.accent = xml->getIntAttribute ("accent", settings.accent);
            settings.timbreColour = xml->getBoolAttribute ("timbreColour", settings.timbreColour);
            settings.crtScreen = xml->getBoolAttribute ("crtScreen", settings.crtScreen);
            settings.spectrum = xml->getBoolAttribute ("spectrum", settings.spectrum);
            settings.motion = xml->getBoolAttribute ("motion", settings.motion);
            settings.analogGlow = xml->getBoolAttribute ("analogGlow", settings.analogGlow);
            settings.shaderFx = xml->getBoolAttribute ("shaderFx", settings.shaderFx);
            settings.particles = xml->getBoolAttribute ("particles", settings.particles);
            settings.scopeMode = xml->getIntAttribute ("scopeMode", settings.scopeMode);
            settings.scopeWindow = xml->getIntAttribute ("scopeWindow", settings.scopeWindow);
        }

        settings.theme = juce::jlimit (0, (int) getThemes().size() - 1, settings.theme);
        settings.accent = juce::jlimit (0, (int) getAccents().size() - 1, settings.accent);
        settings.scopeMode = juce::jlimit (0, 2, settings.scopeMode);
        return settings;
    }

    // Текущая тема: как переносить нейтральные цвета исходной палитры
    float currentHue = -1.0f, currentSaturation = 1.0f;
}

Settings& Settings::get()
{
    static Settings settings = loadSettings();
    return settings;
}

juce::File Settings::getFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Sonder")
               .getChildFile ("Settings.xml");
}

void Settings::save() const
{
    juce::XmlElement xml ("SonderSettings");
    xml.setAttribute ("theme", theme);
    xml.setAttribute ("accent", accent);
    xml.setAttribute ("timbreColour", timbreColour);
    xml.setAttribute ("crtScreen", crtScreen);
    xml.setAttribute ("spectrum", spectrum);
    xml.setAttribute ("motion", motion);
    xml.setAttribute ("analogGlow", analogGlow);
    xml.setAttribute ("shaderFx", shaderFx);
    xml.setAttribute ("particles", particles);
    xml.setAttribute ("scopeMode", scopeMode);
    xml.setAttribute ("scopeWindow", scopeWindow);

    const auto file = getFile();
    if (file.getParentDirectory().createDirectory())
        xml.writeTo (file);
}

const std::vector<ThemeInfo>& getThemes()
{
    static const std::vector<ThemeInfo> themes {
        { "Deep Sea", -1.0f,  1.0f,  kDefaultAccent },
        { "Graphite", -1.0f,  0.16f, 0xfff0a23a },
        { "Midnight",  0.69f, 1.0f,  0xff9b7bff },
        { "Ember",     0.04f, 0.7f,  0xffff8a3d },
    };
    return themes;
}

const std::vector<AccentInfo>& getAccents()
{
    static const std::vector<AccentInfo> accents {
        { "Theme colour", 0 },
        { "Cyan",   kDefaultAccent },
        { "Mint",   0xff3ddc97 },
        { "Lime",   0xffa6e22e },
        { "Amber",  0xfff5a623 },
        { "Coral",  0xffff6b5e },
        { "Pink",   0xffff5fa2 },
        { "Violet", 0xff9b7bff },
        { "Ice",    0xffa9d6ff },
    };
    return accents;
}

juce::Colour themed (juce::uint32 argb)
{
    const juce::Colour colour (argb);
    if (currentHue < 0.0f && currentSaturation == 1.0f)
        return colour;

    float hue = 0.0f, saturation = 0.0f, brightness = 0.0f;
    colour.getHSB (hue, saturation, brightness);
    return juce::Colour::fromHSV (currentHue < 0.0f ? hue : currentHue, juce::jmin (1.0f, saturation * currentSaturation),
                                  brightness, colour.getFloatAlpha());
}

void applyTheme (const Settings& settings)
{
    const auto& theme = getThemes()[(size_t) juce::jlimit (0, (int) getThemes().size() - 1, settings.theme)];
    currentHue = theme.hue;
    currentSaturation = theme.saturation;

    Palette::background  = themed (0xff0a1114);
    Palette::headerTop   = themed (0xff122026);
    Palette::panelTop    = themed (0xff15242a);
    Palette::panelBottom = themed (0xff0f191e);
    Palette::outline     = themed (0xff1f3239);
    Palette::deep        = themed (0xff060d10);
    Palette::track       = themed (0xff1a2b31);
    Palette::knobTop     = themed (0xff273c44);
    Palette::knobBottom  = themed (0xff111c21);
    Palette::knobEdge    = themed (0xff2f4851);
    Palette::text        = themed (0xffd3e5e8);
    Palette::textDim     = themed (0xff7b959c);
    Palette::textFaint   = themed (0xff4a6168);

    const auto& accents = getAccents();
    const int accentIndex = juce::jlimit (0, (int) accents.size() - 1, settings.accent);
    const juce::uint32 accent = accentIndex == 0 ? theme.accent : accents[(size_t) accentIndex].colour;

    Palette::accent = juce::Colour (accent);

    if (accent == kDefaultAccent)
    {
        Palette::accentBright = juce::Colour (0xff8ae8f2);
        Palette::accentDim = juce::Colour (0xff1c6f79);
    }
    else
    {
        // Светлый и тёмный варианты в тех же пропорциях, что у исходного бирюзового
        float hue = 0.0f, saturation = 0.0f, brightness = 0.0f;
        Palette::accent.getHSB (hue, saturation, brightness);
        Palette::accentBright = juce::Colour::fromHSV (hue, saturation * 0.56f, juce::jmax (0.95f, brightness), 1.0f);
        Palette::accentDim = juce::Colour::fromHSV (hue, saturation, brightness * 0.6f, 1.0f);
    }
}

juce::Colour shiftColour (juce::Colour base, float t)
{
    if (std::abs (t) < 0.004f)
        return base;

    float hue = 0.0f, saturation = 0.0f, brightness = 0.0f;
    base.getHSB (hue, saturation, brightness);

    // "Холодная" сторона - синий; идём к нему по кратчайшей дуге цветового круга
    float toCold = 0.667f - hue;
    if (toCold > 0.5f)  toCold -= 1.0f;
    if (toCold < -0.5f) toCold += 1.0f;
    const float direction = toCold >= 0.0f ? 1.0f : -1.0f;

    if (t < 0.0f)
    {
        const float amount = juce::jmin (1.0f, -t);
        hue += direction * juce::jmin (std::abs (toCold), 0.085f) * amount;
        saturation = juce::jmin (1.0f, saturation * (1.0f + 0.2f * amount));
        brightness *= 1.0f - 0.12f * amount;
    }
    else
    {
        const float amount = juce::jmin (1.0f, t);
        hue -= direction * 0.05f * amount;
        saturation *= 1.0f - 0.5f * amount;
        brightness += (1.0f - brightness) * 0.6f * amount;
    }

    hue -= std::floor (hue);
    return juce::Colour::fromHSV (hue, saturation, brightness, base.getFloatAlpha());
}

} // namespace sonder::ui
