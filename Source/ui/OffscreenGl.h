#pragma once

namespace sonder::ui
{

// Контекст OpenGL без окна на экране: нужен только для того, чтобы прогонять картинки через шейдеры.
// Окно плагина к нему отношения не имеет и рисуется как обычно, поэтому в DAW здесь нечему ломаться.
// Живёт и используется на message thread.
class OffscreenGl
{
public:
    OffscreenGl();
    ~OffscreenGl();

    bool isValid() const noexcept { return renderContext != nullptr; }

    // Делает контекст текущим на время жизни объекта и возвращает прежний: на message thread
    // хоста может быть активен чужой контекст, его нельзя сбивать
    class Scope
    {
    public:
        explicit Scope (OffscreenGl& context);
        ~Scope();

    private:
        void* previousDevice = nullptr;
        void* previousContext = nullptr;
    };

private:
    void* window = nullptr;
    void* deviceContext = nullptr;
    void* renderContext = nullptr;
};

} // namespace sonder::ui
