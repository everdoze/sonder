#pragma once

#include "OffscreenGl.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sonder::ui
{

// Настройки шейдерной обработки одного экрана
struct ScreenFx
{
    float bloom = 0.0f;        // сила размытого свечения вокруг ярких линий
    float curvature = 0.0f;    // выпуклость стекла: доля меньшей стороны, на которую "уезжают" углы
    float aberration = 0.0f;   // расслоение цвета к краям, в пикселях на углу экрана
    float scanlines = 0.0f;    // глубина строк развёртки
    float vignette = 0.0f;     // затемнение к краям
    float persistence = 0.0f;  // какая доля яркости остаётся от прошлого кадра (0 - без послесвечения)
    float noise = 0.0f;        // зерно и дрожание
    float time = 0.0f;         // секунды, для зерна
    float scale = 1.0f;        // физических пикселей на единицу координат компонента
    float cornerRadius = 6.0f; // скругление углов экрана, в координатах компонента
    float inset = 1.0f;        // отступ экрана от края компонента, в координатах компонента
};

// Обработка картинок шейдерами на видеокарте: послесвечение, bloom, выпуклое стекло кинескопа.
// Работает в собственном контексте OpenGL без окна; результат возвращается обычной картинкой,
// которую экран рисует как всегда. Один на весь процесс, используется только с message thread.
class ShaderStage
{
public:
    ShaderStage();
    ~ShaderStage();

    // false: видеокарта или драйвер не подошли, экраны рисуются программно; причина в getStatus()
    bool isAvailable() const noexcept { return ready; }
    const juce::String& getStatus() const noexcept { return status; }

    // Текстуры одного экрана. Создаётся и уничтожается, пока жива ShaderStage.
    class Screen
    {
    public:
        explicit Screen (ShaderStage& stage);
        ~Screen();

        // source - программная ARGB-картинка; возвращает обработанную картинку того же размера
        // (она принадлежит экрану и переписывается при следующем вызове).
        // Обычно результат забирается кадром позже: видеокарта успевает досчитать, и ждать её не нужно.
        // immediate = true - результат этого же кадра (для отклика на мышь), ценой ожидания.
        const juce::Image& process (const juce::Image& source, const ScreenFx& fx, bool immediate = false);

        // Забирает результат последнего process(), если он ещё не был забран (без нового расчёта).
        // Нужно экранам, которые перерисовываются только при изменениях: иначе последний кадр
        // так и остался бы непоказанным. true - картинка обновилась.
        bool flush();

    private:
        struct Buffers;

        ShaderStage& stage;
        std::unique_ptr<Buffers> buffers;
        juce::Image output;
    };

private:
    struct Programs;

    OffscreenGl context;
    std::unique_ptr<Programs> programs;
    bool ready = false;
    juce::String status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaderStage)
};

// То, что держит у себя каждый экран: общая ShaderStage, свои текстуры и картинки.
// Экран из своего таймера вызывает render(), а в paint() - draw().
class ShaderScreen
{
public:
    bool isAvailable() const noexcept { return stage->isAvailable(); }
    const juce::String& getStatus() const noexcept { return stage->getStatus(); }

    // Разрешение исходника. Animated: экран перерисовывается каждый кадр, поэтому в крупном окне разрешение
    // ограничено (шейдеры всё равно смягчают картинку, подписи рисуются поверх в полном разрешении).
    // Native: экран перерисовывается только при изменениях - полное разрешение окна, без мыла.
    // Supersampled: вдвое больше пикселей, чем в окне (тонкие линии после изгиба стекла не рябят).
    enum class Detail { animated, native, supersampled };

    // Рисует содержимое экрана функцией paint (в координатах компонента) и прогоняет через шейдеры.
    // immediate - см. ShaderStage::Screen::process.
    template <typename Paint>
    void render (const juce::Component& owner, ScreenFx fx, Paint&& paint, bool immediate = false,
                 Detail detail = Detail::animated)
    {
        const float renderScale = detail == Detail::animated ? juce::jmin (scale, kMaxRenderScale)
                                : detail == Detail::native   ? scale
                                                             : juce::jmin (scale * 2.0f, kMaxSupersampledScale);
        const int width = juce::roundToInt ((float) owner.getWidth() * renderScale);
        const int height = juce::roundToInt ((float) owner.getHeight() * renderScale);

        if (! stage->isAvailable() || width < 8 || height < 8)
        {
            invalidate();
            return;
        }

        if (source.getWidth() != width || source.getHeight() != height)
            source = juce::Image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
        else
            source.clear (source.getBounds());

        {
            juce::Graphics g (source);
            g.addTransform (juce::AffineTransform::scale (renderScale));
            paint (g);
        }

        if (screen == nullptr)
            screen = std::make_unique<ShaderStage::Screen> (*stage);

        fx.scale = renderScale;
        output = screen->process (source, fx, immediate);
    }

    bool isActive() const noexcept { return output.isValid(); }

    // См. ShaderStage::Screen::flush
    bool flush() { return screen != nullptr && output.isValid() && screen->flush(); }

    // Рисует обработанную картинку; false - её нет, экран должен нарисовать себя сам
    bool draw (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Масштаб окна узнаём здесь и используем при следующем render()
        scale = juce::jlimit (0.5f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

        if (! output.isValid())
            return false;

        g.setOpacity (1.0f);
        g.drawImage (output, bounds);
        return true;
    }

    void invalidate() { output = {}; }

private:
    static constexpr float kMaxRenderScale = 1.25f;
    static constexpr float kMaxSupersampledScale = 3.0f;

    juce::SharedResourcePointer<ShaderStage> stage;
    std::unique_ptr<ShaderStage::Screen> screen;
    juce::Image source, output;
    float scale = 1.0f;
};

} // namespace sonder::ui
