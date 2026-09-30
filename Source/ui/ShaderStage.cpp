#include "ShaderStage.h"

#include <juce_opengl/juce_opengl.h>

namespace sonder::ui
{

using namespace juce::gl;

namespace
{
    // Во всех текстурах строка 0 - верх картинки (как в памяти), поэтому переворачивать ничего не нужно:
    // проходы рисуют прямоугольник во весь кадр один к одному.
    const char* const vertexShader = R"(
        #version 120
        attribute vec2 position;
        varying vec2 uv;

        void main()
        {
            uv = position * 0.5 + 0.5;
            gl_Position = vec4 (position, 0.0, 1.0);
        }
    )";

    // Послесвечение люминофора: от прошлого кадра остаётся гаснущий след
    const char* const persistShader = R"(
        #version 120
        uniform sampler2D source;
        uniform sampler2D previous;
        uniform float decay;
        varying vec2 uv;

        void main()
        {
            vec4 now = texture2D (source, uv);
            vec4 before = max (texture2D (previous, uv) * decay - vec4 (0.004), vec4 (0.0));
            gl_FragColor = max (now, before);
        }
    )";

    // Яркие места в половинном разрешении: из них получается свечение
    const char* const brightShader = R"(
        #version 120
        uniform sampler2D image;
        uniform vec2 texel;
        varying vec2 uv;

        void main()
        {
            vec3 c = 0.25 * (texture2D (image, uv + texel * vec2 (-1.0, -1.0)).rgb
                           + texture2D (image, uv + texel * vec2 ( 1.0, -1.0)).rgb
                           + texture2D (image, uv + texel * vec2 (-1.0,  1.0)).rgb
                           + texture2D (image, uv + texel * vec2 ( 1.0,  1.0)).rgb);

            float peak = max (c.r, max (c.g, c.b));
            gl_FragColor = vec4 (c * smoothstep (0.18, 0.7, peak), 1.0);
        }
    )";

    // Размытие по Гауссу в одну сторону (9 отсчётов за 5 выборок)
    const char* const blurShader = R"(
        #version 120
        uniform sampler2D image;
        uniform vec2 direction;
        varying vec2 uv;

        void main()
        {
            vec3 c = texture2D (image, uv).rgb * 0.2270270270;
            c += (texture2D (image, uv + direction * 1.3846153846).rgb
                + texture2D (image, uv - direction * 1.3846153846).rgb) * 0.3162162162;
            c += (texture2D (image, uv + direction * 3.2307692308).rgb
                + texture2D (image, uv - direction * 3.2307692308).rgb) * 0.0702702703;
            gl_FragColor = vec4 (c, 1.0);
        }
    )";

    // Итог: выпуклое стекло, расслоение цвета, свечение, строки развёртки, затемнение к краям, зерно, блик
    const char* const compositeShader = R"(
        #version 120
        uniform sampler2D image;
        uniform sampler2D glowNear;
        uniform sampler2D glowFar;
        uniform vec2 size;
        uniform vec2 bend;
        uniform float bloom;
        uniform float aberration;
        uniform float scanlines;
        uniform float vignette;
        uniform float noise;
        uniform float time;
        uniform float pixelScale;
        uniform float cornerRadius;
        uniform float inset;
        varying vec2 uv;

        float hash (vec2 p)
        {
            return fract (sin (dot (p, vec2 (12.9898, 78.233))) * 43758.5453);
        }

        void main()
        {
            vec2 c = uv * 2.0 - 1.0;

            // Выпуклое стекло: чем ближе к углу, тем сильнее картинка уходит за край
            vec2 p = c * (1.0 + bend * c.yx * c.yx) * 0.5 + 0.5;

            vec2 shift = c * dot (c, c) * 0.5 * aberration / size;
            vec3 col = vec3 (texture2D (image, p + shift).r,
                             texture2D (image, p).g,
                             texture2D (image, p - shift).b);

            col += (texture2D (glowNear, p).rgb * 0.5 + texture2D (glowFar, p).rgb * 1.2) * bloom;

            float line = 0.5 + 0.5 * cos (6.2831853 * p.y * size.y / (3.0 * pixelScale));
            col *= (1.0 - scanlines * line) * (1.0 + scanlines * 0.45);

            float edge = max (smoothstep (0.35, 1.0, abs (c.y)), smoothstep (0.78, 1.0, abs (c.x)));
            col *= 1.0 - vignette * edge;

            float grain = hash (uv * size + vec2 (time * 37.0, time * 91.0)) - 0.5;
            col += grain * noise * (0.3 + max (col.r, max (col.g, col.b)));

            // Блик на стекле в левом верхнем углу
            float glare = clamp (1.0 - length ((uv - vec2 (0.1, 0.0)) * vec2 (1.3, 2.4)), 0.0, 1.0);
            col += vec3 (0.045) * glare * glare;

            // За краем искривлённой картинки - чёрная рамка трубки
            vec2 border = min (p, 1.0 - p) * size;
            col *= clamp (min (border.x, border.y) + 0.5, 0.0, 1.0);

            // Скруглённые углы экрана
            vec2 q = abs (uv * size - size * 0.5) - (size * 0.5 - vec2 (inset + cornerRadius));
            float outside = length (max (q, 0.0)) - cornerRadius;
            float mask = clamp (0.5 - outside, 0.0, 1.0);

            gl_FragColor = vec4 (clamp (col, 0.0, 1.0) * mask, mask);
        }
    )";

    juce::String shaderLog (GLuint shader)
    {
        GLint length = 0;
        glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<GLchar> text ((size_t) juce::jmax (1, length) + 1, 0);
        glGetShaderInfoLog (shader, (GLsizei) text.size() - 1, nullptr, text.data());
        return juce::String::fromUTF8 (text.data()).trim();
    }

    GLuint compile (GLenum type, const char* code, juce::String& error)
    {
        const GLuint shader = glCreateShader (type);
        glShaderSource (shader, 1, &code, nullptr);
        glCompileShader (shader);

        GLint ok = GL_FALSE;
        glGetShaderiv (shader, GL_COMPILE_STATUS, &ok);

        if (ok != GL_TRUE)
        {
            error = shaderLog (shader);
            glDeleteShader (shader);
            return 0;
        }

        return shader;
    }

    // Программа из общего вершинного шейдера и своего фрагментного; 0 при ошибке
    GLuint makeProgram (const char* fragmentCode, juce::String& error)
    {
        const GLuint vertex = compile (GL_VERTEX_SHADER, vertexShader, error);
        const GLuint fragment = vertex != 0 ? compile (GL_FRAGMENT_SHADER, fragmentCode, error) : 0;

        if (vertex == 0 || fragment == 0)
        {
            if (vertex != 0)
                glDeleteShader (vertex);

            return 0;
        }

        const GLuint program = glCreateProgram();
        glAttachShader (program, vertex);
        glAttachShader (program, fragment);
        glBindAttribLocation (program, 0, "position");
        glLinkProgram (program);
        glDeleteShader (vertex);
        glDeleteShader (fragment);

        GLint ok = GL_FALSE;
        glGetProgramiv (program, GL_LINK_STATUS, &ok);

        if (ok != GL_TRUE)
        {
            error = "link failed";
            glDeleteProgram (program);
            return 0;
        }

        return program;
    }

    void setUniform (GLuint program, const char* name, float value)
    {
        glUniform1f (glGetUniformLocation (program, name), value);
    }

    void setUniform (GLuint program, const char* name, float x, float y)
    {
        glUniform2f (glGetUniformLocation (program, name), x, y);
    }

    void setTexture (GLuint program, const char* name, int unit, GLuint texture)
    {
        glActiveTexture ((GLenum) (GL_TEXTURE0 + unit));
        glBindTexture (GL_TEXTURE_2D, texture);
        glUniform1i (glGetUniformLocation (program, name), unit);
    }

    // Текстура с буфером кадра: в неё можно рисовать и из неё можно читать
    struct Surface
    {
        GLuint texture = 0, frameBuffer = 0;
        int width = 0, height = 0;

        bool resize (int newWidth, int newHeight)
        {
            if (texture != 0 && width == newWidth && height == newHeight)
                return true;

            release();
            width = newWidth;
            height = newHeight;

            glGenTextures (1, &texture);
            glBindTexture (GL_TEXTURE_2D, texture);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

            glGenFramebuffers (1, &frameBuffer);
            glBindFramebuffer (GL_FRAMEBUFFER, frameBuffer);
            glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

            const bool complete = glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
            glClear (GL_COLOR_BUFFER_BIT);
            return complete;
        }

        void release()
        {
            if (frameBuffer != 0)
                glDeleteFramebuffers (1, &frameBuffer);

            if (texture != 0)
                glDeleteTextures (1, &texture);

            frameBuffer = texture = 0;
        }

        void target() const
        {
            glBindFramebuffer (GL_FRAMEBUFFER, frameBuffer);
            glViewport (0, 0, width, height);
        }
    };
}

//==============================================================================
struct ShaderStage::Programs
{
    GLuint persist = 0, bright = 0, blur = 0, composite = 0;
    GLuint quad = 0;

    bool create (juce::String& error)
    {
        const std::pair<GLuint*, const char*> list[] { { &persist, persistShader }, { &bright, brightShader },
                                                       { &blur, blurShader }, { &composite, compositeShader } };
        for (const auto& [program, code] : list)
        {
            *program = makeProgram (code, error);
            if (*program == 0)
                return false;
        }

        static const GLfloat corners[] { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
        glGenBuffers (1, &quad);
        glBindBuffer (GL_ARRAY_BUFFER, quad);
        glBufferData (GL_ARRAY_BUFFER, sizeof (corners), corners, GL_STATIC_DRAW);
        return true;
    }

    void release()
    {
        for (GLuint program : { persist, bright, blur, composite })
            if (program != 0)
                glDeleteProgram (program);

        if (quad != 0)
            glDeleteBuffers (1, &quad);
    }

    // Рисует прямоугольник во весь текущий буфер кадра активной программой
    void draw() const
    {
        glBindBuffer (GL_ARRAY_BUFFER, quad);
        glEnableVertexAttribArray (0);
        glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    }
};

ShaderStage::ShaderStage()
{
    if (! context.isValid())
    {
        status = "no OpenGL context";
        return;
    }

    const OffscreenGl::Scope scope (context);
    loadFunctions();

    // Нужны шейдеры и буферы кадра (OpenGL 3.0 или соответствующие расширения)
    if (glCreateShader == nullptr || glGenFramebuffers == nullptr || glGenBuffers == nullptr || glActiveTexture == nullptr)
    {
        status = "OpenGL 3.0 is required";
        return;
    }

    programs = std::make_unique<Programs>();
    juce::String error;

    if (! programs->create (error))
    {
        status = "shader error: " + error;
        programs->release();
        programs.reset();
        return;
    }

    if (const auto* renderer = glGetString (GL_RENDERER))
        status = juce::String::fromUTF8 ((const char*) renderer);

    ready = true;
}

ShaderStage::~ShaderStage()
{
    if (programs != nullptr)
    {
        const OffscreenGl::Scope scope (context);
        programs->release();
    }
}

//==============================================================================
struct ShaderStage::Screen::Buffers
{
    Surface source, persistA, persistB, glowA, glowB, farA, farB, result;
    bool persistFlip = false, hadPersistence = false;

    // Два буфера для чтения результата: в один видеокарта пишет сейчас, из другого забираем прошлый кадр
    GLuint readBuffers[2] {};
    int readWidth = 0, readHeight = 0, readNext = 0, readPending = -1;

    void releaseReadBuffers()
    {
        if (readBuffers[0] != 0)
            glDeleteBuffers (2, readBuffers);

        readBuffers[0] = readBuffers[1] = 0;
        readPending = -1;
    }

    void release()
    {
        for (auto* surface : { &source, &persistA, &persistB, &glowA, &glowB, &farA, &farB, &result })
            surface->release();

        releaseReadBuffers();
    }
};

ShaderStage::Screen::Screen (ShaderStage& s)
    : stage (s), buffers (std::make_unique<Buffers>())
{
}

ShaderStage::Screen::~Screen()
{
    if (stage.ready)
    {
        const OffscreenGl::Scope scope (stage.context);
        buffers->release();
    }
}

bool ShaderStage::Screen::flush()
{
    auto& b = *buffers;

    if (! stage.ready || b.readPending < 0 || ! output.isValid()
        || output.getWidth() != b.readWidth || output.getHeight() != b.readHeight)
        return false;

    const OffscreenGl::Scope scope (stage.context);
    const int width = b.readWidth, height = b.readHeight;
    bool delivered = false;

    glBindBuffer (GL_PIXEL_PACK_BUFFER, b.readBuffers[b.readPending]);

    if (const auto* mapped = static_cast<const juce::uint8*> (glMapBuffer (GL_PIXEL_PACK_BUFFER, GL_READ_ONLY)))
    {
        juce::Image::BitmapData data (output, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < height; ++y)
            std::memcpy (data.getLinePointer (y), mapped + (size_t) y * (size_t) width * 4, (size_t) width * 4);

        delivered = true;
    }

    glUnmapBuffer (GL_PIXEL_PACK_BUFFER);
    glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);

    // Этот результат показан; следующий process() прочитает свой кадр сам
    b.readPending = -1;
    return delivered;
}

const juce::Image& ShaderStage::Screen::process (const juce::Image& source, const ScreenFx& fx, bool immediate)
{
    const int width = source.getWidth(), height = source.getHeight();

    if (! stage.ready || width < 2 || height < 2)
    {
        output = {};
        return output;
    }

    const OffscreenGl::Scope scope (stage.context);
    const auto& pass = *stage.programs;
    auto& b = *buffers;

    const int halfWidth = juce::jmax (1, width / 2), halfHeight = juce::jmax (1, height / 2);
    const int farWidth = juce::jmax (1, width / 4), farHeight = juce::jmax (1, height / 4);

    const bool complete = b.source.resize (width, height) && b.result.resize (width, height)
                       && b.persistA.resize (width, height) && b.persistB.resize (width, height)
                       && b.glowA.resize (halfWidth, halfHeight) && b.glowB.resize (halfWidth, halfHeight)
                       && b.farA.resize (farWidth, farHeight) && b.farB.resize (farWidth, farHeight);

    if (! complete)
    {
        output = {};
        return output;
    }

    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_SCISSOR_TEST);

    // Исходная картинка: в памяти она лежит как BGRA, строки сверху вниз
    {
        const juce::Image::BitmapData data (source, juce::Image::BitmapData::readOnly);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, b.source.texture);
        glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei (GL_UNPACK_ROW_LENGTH, data.lineStride / data.pixelStride);
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, data.data);
        glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    }

    // Послесвечение: в одну текстуру пишем, из другой читаем прошлый кадр
    GLuint picture = b.source.texture;

    if (fx.persistence > 0.0f)
    {
        auto& current = b.persistFlip ? b.persistA : b.persistB;
        auto& previous = b.persistFlip ? b.persistB : b.persistA;
        b.persistFlip = ! b.persistFlip;

        if (! b.hadPersistence)
        {
            // След от давно прошедших кадров не нужен
            previous.target();
            glClear (GL_COLOR_BUFFER_BIT);
        }

        current.target();
        glUseProgram (pass.persist);
        setTexture (pass.persist, "source", 0, b.source.texture);
        setTexture (pass.persist, "previous", 1, previous.texture);
        setUniform (pass.persist, "decay", juce::jlimit (0.0f, 0.98f, fx.persistence));
        pass.draw();
        picture = current.texture;
    }

    b.hadPersistence = fx.persistence > 0.0f;

    // Свечение: яркие места, размытые в половинном и в четвертном разрешении
    if (fx.bloom > 0.0f)
    {
        b.glowA.target();
        glUseProgram (pass.bright);
        setTexture (pass.bright, "image", 0, picture);
        setUniform (pass.bright, "texel", 1.0f / (float) width, 1.0f / (float) height);
        pass.draw();

        const auto blur = [&pass] (const Surface& from, const Surface& to, float dx, float dy)
        {
            to.target();
            setTexture (pass.blur, "image", 0, from.texture);
            setUniform (pass.blur, "direction", dx / (float) from.width, dy / (float) from.height);
            pass.draw();
        };

        glUseProgram (pass.blur);
        blur (b.glowA, b.glowB, 1.0f, 0.0f);
        blur (b.glowB, b.glowA, 0.0f, 1.0f);
        blur (b.glowA, b.farB, 1.0f, 0.0f);
        blur (b.farB, b.farA, 0.0f, 1.0f);
        blur (b.farA, b.farB, 1.6f, 0.0f);
        blur (b.farB, b.farA, 0.0f, 1.6f);
    }

    // Итоговый проход
    b.result.target();
    glUseProgram (pass.composite);
    setTexture (pass.composite, "image", 0, picture);
    setTexture (pass.composite, "glowNear", 1, b.glowA.texture);
    setTexture (pass.composite, "glowFar", 2, b.farA.texture);

    const float corner = fx.curvature * (float) juce::jmin (width, height);
    setUniform (pass.composite, "size", (float) width, (float) height);
    setUniform (pass.composite, "bend", 2.0f * corner / (float) width, 2.0f * corner / (float) height);
    setUniform (pass.composite, "bloom", fx.bloom);
    setUniform (pass.composite, "aberration", fx.aberration * fx.scale);
    setUniform (pass.composite, "scanlines", fx.scanlines);
    setUniform (pass.composite, "vignette", fx.vignette);
    setUniform (pass.composite, "noise", fx.noise);
    setUniform (pass.composite, "time", std::fmod (fx.time, 100.0f));
    setUniform (pass.composite, "pixelScale", fx.scale);
    setUniform (pass.composite, "cornerRadius", fx.cornerRadius * fx.scale);
    setUniform (pass.composite, "inset", fx.inset * fx.scale);
    pass.draw();

    // Результат обратно в обычную картинку
    // Родная для окна картинка: при отрисовке Direct2D только дозаливает её пиксели, а не создаёт заново
    if (output.getWidth() != width || output.getHeight() != height)
        output = juce::Image (juce::Image::ARGB, width, height, false, juce::NativeImageType());

    const auto bytes = (GLsizeiptr) width * height * 4;
    const bool asynchronous = ! immediate && glMapBuffer != nullptr && glUnmapBuffer != nullptr;

    if (b.readWidth != width || b.readHeight != height || ! asynchronous)
    {
        b.releaseReadBuffers();
        b.readWidth = width;
        b.readHeight = height;
    }

    glPixelStorei (GL_PACK_ALIGNMENT, 4);
    glPixelStorei (GL_PACK_ROW_LENGTH, 0);

    bool delivered = false;

    if (asynchronous)
    {
        if (b.readBuffers[0] == 0)
        {
            glGenBuffers (2, b.readBuffers);
            for (GLuint buffer : b.readBuffers)
            {
                glBindBuffer (GL_PIXEL_PACK_BUFFER, buffer);
                glBufferData (GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
            }
        }

        // Этот кадр: копирование на стороне видеокарты, без ожидания
        glBindBuffer (GL_PIXEL_PACK_BUFFER, b.readBuffers[b.readNext]);
        glReadPixels (0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

        // Прошлый кадр: к этому времени он уже готов
        if (b.readPending >= 0)
        {
            glBindBuffer (GL_PIXEL_PACK_BUFFER, b.readBuffers[b.readPending]);

            if (const auto* mapped = static_cast<const juce::uint8*> (glMapBuffer (GL_PIXEL_PACK_BUFFER, GL_READ_ONLY)))
            {
                juce::Image::BitmapData data (output, juce::Image::BitmapData::writeOnly);
                for (int y = 0; y < height; ++y)
                    std::memcpy (data.getLinePointer (y), mapped + (size_t) y * (size_t) width * 4, (size_t) width * 4);

                delivered = true;
            }

            glUnmapBuffer (GL_PIXEL_PACK_BUFFER);
        }

        glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
        b.readPending = b.readNext;
        b.readNext ^= 1;
    }

    // Первый кадр или нужен результат прямо сейчас: читаем с ожиданием
    if (! delivered)
    {
        juce::Image::BitmapData data (output, juce::Image::BitmapData::writeOnly);
        glPixelStorei (GL_PACK_ROW_LENGTH, data.lineStride / data.pixelStride);
        glReadPixels (0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, data.data);
        glPixelStorei (GL_PACK_ROW_LENGTH, 0);
    }

    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glUseProgram (0);
    return output;
}

} // namespace sonder::ui
