#ifndef TEXTURE_GL_UTILS_H
#define TEXTURE_GL_UTILS_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QStringList>
#include <string>

namespace parafashion_tex {

// Converts a QImage (top-to-bottom scanlines, arbitrary pixel format) into a tightly
// packed bottom-to-top QImage::Format_RGBA8888 buffer suitable for OpenGL glTexImage2D
// with format=GL_RGBA, type=GL_UNSIGNED_BYTE and Wavefront OBJ bottom-left UV origin (0,0).
inline QImage prepareTextureForGL(const QImage &src)
{
    if (src.isNull() || src.width() <= 0 || src.height() <= 0) {
        return QImage();
    }
    return src.convertToFormat(QImage::Format_RGBA8888).mirrored(false, true);
}

namespace detail {

inline QString readNextToken(const QString &str, int &pos)
{
    const int n = str.size();
    while (pos < n && str[pos].isSpace()) {
        ++pos;
    }
    const int start = pos;
    while (pos < n && !str[pos].isSpace()) {
        ++pos;
    }
    return str.mid(start, pos - start);
}

inline bool hasRemainingNonSpace(const QString &str, int pos)
{
    const int n = str.size();
    while (pos < n) {
        if (!str[pos].isSpace()) {
            return true;
        }
        ++pos;
    }
    return false;
}

// Extracts the texture filename from the arguments of a Wavefront MTL `map_Kd` directive,
// skipping optional MTL texture flags (e.g. -bm, -s, -o, -t, -mm, -clamp, etc.).
inline QString parseMtlMapKdValue(const QString &rawArgs)
{
    int pos = 0;
    const int n = rawArgs.size();
    while (pos < n && rawArgs[pos].isSpace()) {
        ++pos;
    }

    while (pos < n && rawArgs[pos] == QLatin1Char('-')) {
        const QString opt = readNextToken(rawArgs, pos).toLower();
        if (opt == QLatin1String("-o") || opt == QLatin1String("-s") || opt == QLatin1String("-t")) {
            // 1 mandatory numeric arg + up to 2 optional numeric args (as long as filename remains)
            readNextToken(rawArgs, pos);
            for (int k = 0; k < 2; ++k) {
                int savedPos = pos;
                const QString candidate = readNextToken(rawArgs, pos);
                bool isNum = false;
                candidate.toDouble(&isNum);
                if (!isNum || !hasRemainingNonSpace(rawArgs, pos)) {
                    pos = savedPos;
                    break;
                }
            }
        } else if (opt == QLatin1String("-mm")) {
            readNextToken(rawArgs, pos);
            readNextToken(rawArgs, pos);
        } else {
            // Single-argument options: -bm, -clamp, -blendu, -blendv, -texres, -imfchan, -cc, -boost, -type
            readNextToken(rawArgs, pos);
        }
        while (pos < n && rawArgs[pos].isSpace()) {
            ++pos;
        }
    }

    QString texRel = rawArgs.mid(pos).trimmed();
    if (texRel.size() >= 2 &&
        ((texRel.startsWith(QLatin1Char('"')) && texRel.endsWith(QLatin1Char('"'))) ||
         (texRel.startsWith(QLatin1Char('\'')) && texRel.endsWith(QLatin1Char('\''))))) {
        texRel = texRel.mid(1, texRel.size() - 2).trimmed();
    }
    texRel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return texRel;
}

inline QString resolveRelativeFile(const QString &baseDir, const QString &relPath)
{
    if (relPath.isEmpty()) {
        return QString();
    }
    const QFileInfo relInfo(relPath);
    if (relInfo.isAbsolute()) {
        return QDir::cleanPath(relPath);
    }
    if (baseDir.isEmpty()) {
        return QDir::cleanPath(relPath);
    }
    return QDir::cleanPath(QDir(baseDir).filePath(relPath));
}

} // namespace detail

// Resolves the diffuse texture path for a mesh:
//   1. <meshDir>/txt.png (Parafashion primary convention)
//   2. Fallback for .obj meshes: parses `mtllib` (and `<stem>.mtl`) -> `map_Kd` relative to the .mtl/.obj directory
//   3. Empty string if no readable image file exists.
inline std::string resolveMeshTexturePath(const std::string &meshPath)
{
    if (meshPath.empty()) {
        return std::string();
    }

    QString normMesh = QString::fromStdString(meshPath).trimmed();
    normMesh.replace(QLatin1Char('\\'), QLatin1Char('/'));

    QString baseDir;
    const int lastSlash = normMesh.lastIndexOf(QLatin1Char('/'));
    if (lastSlash == 0) {
        baseDir = QStringLiteral("/");
    } else if (lastSlash > 0) {
        baseDir = normMesh.left(lastSlash);
    }

    // 1. Primary Parafashion convention: <baseDir>/txt.png
    const QString txtPngCandidate = detail::resolveRelativeFile(baseDir, QStringLiteral("txt.png"));
    if (QFileInfo(txtPngCandidate).isFile()) {
        return txtPngCandidate.toStdString();
    }

    // 2. Fallback: parse Wavefront OBJ `mtllib` -> MTL `map_Kd`
    const QFileInfo meshInfo(normMesh);
    if (!meshInfo.isFile() || meshInfo.suffix().toLower() != QLatin1String("obj")) {
        return std::string();
    }

    QStringList mtlPaths;
    QFile objFile(normMesh);
    if (objFile.open(QIODevice::ReadOnly)) {
        while (!objFile.atEnd()) {
            const QByteArray rawLine = objFile.readLine();
            int i = 0;
            while (i < rawLine.size() && (rawLine[i] == ' ' || rawLine[i] == '\t')) {
                ++i;
            }
            if (rawLine.size() - i >= 7 &&
                qstrnicmp(rawLine.constData() + i, "mtllib", 6) == 0 &&
                (rawLine[i + 6] == ' ' || rawLine[i + 6] == '\t')) {
                QString mtlRel = QString::fromUtf8(rawLine.mid(i + 6)).trimmed();
                if (mtlRel.size() >= 2 &&
                    mtlRel.startsWith(QLatin1Char('"')) && mtlRel.endsWith(QLatin1Char('"'))) {
                    mtlRel = mtlRel.mid(1, mtlRel.size() - 2).trimmed();
                }
                mtlRel.replace(QLatin1Char('\\'), QLatin1Char('/'));
                const QString resolvedMtl = detail::resolveRelativeFile(baseDir, mtlRel);
                if (!resolvedMtl.isEmpty() && !mtlPaths.contains(resolvedMtl)) {
                    mtlPaths.append(resolvedMtl);
                }
            }
        }
        objFile.close();
    }

    const QString stemMtl =
        detail::resolveRelativeFile(baseDir, meshInfo.completeBaseName() + QStringLiteral(".mtl"));
    if (!stemMtl.isEmpty() && !mtlPaths.contains(stemMtl)) {
        mtlPaths.append(stemMtl);
    }

    for (const QString &mtlPath : mtlPaths) {
        const QFileInfo mtlInfo(mtlPath);
        if (!mtlInfo.isFile()) {
            continue;
        }
        const QString mtlDir = mtlInfo.path();
        QFile mtlFile(mtlPath);
        if (!mtlFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        while (!mtlFile.atEnd()) {
            const QString line = QString::fromUtf8(mtlFile.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            int pos = 0;
            const QString keyword = detail::readNextToken(line, pos).toLower();
            if (keyword != QLatin1String("map_kd")) {
                continue;
            }
            const QString texRel = detail::parseMtlMapKdValue(line.mid(pos));
            if (texRel.isEmpty()) {
                continue;
            }
            const QString candidateFromMtl = detail::resolveRelativeFile(mtlDir, texRel);
            if (QFileInfo(candidateFromMtl).isFile()) {
                return candidateFromMtl.toStdString();
            }
            const QString candidateFromObj = detail::resolveRelativeFile(baseDir, texRel);
            if (QFileInfo(candidateFromObj).isFile()) {
                return candidateFromObj.toStdString();
            }
        }
        mtlFile.close();
    }

    return std::string();
}

// Returns the OpenGL texture object ID to pass to MeshDrawing::GLDrawUV (int TxtIndex),
// or -1 when texturing is disabled or no valid texture is loaded.
inline int resolveUVTextureIndex(bool hasTxt, bool isTextured, unsigned int textureId)
{
    if (hasTxt && isTextured && textureId > 0u) {
        return static_cast<int>(textureId);
    }
    return -1;
}

#ifdef GL_TEXTURE_2D
// Uploads a QImage into a 2D OpenGL texture (reusing texId if already allocated to prevent
// leaking GL textures across resizeGL/initializeGL or repeated layout updates).
inline bool uploadQImageTexture2D(GLuint &texId, const QImage &src)
{
    const QImage glImg = prepareTextureForGL(src);
    if (glImg.isNull()) {
        return false;
    }
    if (texId == 0 || !glIsTexture(texId)) {
        glGenTextures(1, &texId);
    }
    GLint prevUnpackAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpackAlignment);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 glImg.width(), glImg.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, glImg.constBits());
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpackAlignment);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}
#endif

} // namespace parafashion_tex

#endif // TEXTURE_GL_UTILS_H
