#include <GL/glew.h>
#include <tracing/mesh_type.h>
#include <vcg/complex/algorithms/closest.h>
#include <vcg/complex/algorithms/parametrization/uv_utils.h>
#include <vcg/space/index/grid_static_ptr.h>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include "pattern_texture.h"

namespace parafashion_tex {
namespace {

bool finiteUV(const vcg::Point2d &uv)
{
    return std::isfinite(uv.X()) && std::isfinite(uv.Y());
}

QPointF patternPoint(const TraceMesh::FaceType &face, int corner, const QPointF &shift, double scale)
{
    const auto &uv = face.cWT(corner).P();
    return shift + QPointF(uv.X() * scale, uv.Y() * scale);
}

double cross(const QPointF &a, const QPointF &b)
{
    return a.x() * b.y() - a.y() * b.x();
}

// Match the OpenGL sampler (repeat and linear filtering, with V=0 at the bottom).
QRgb sampleTexture(const QImage &image, double u, double v)
{
    const double x = (u - std::floor(u)) * image.width() - 0.5;
    const double y = (1.0 - (v - std::floor(v))) * image.height() - 0.5;
    const int left = static_cast<int>(std::floor(x));
    const int top = static_cast<int>(std::floor(y));
    const double dx = x - left;
    const double dy = y - top;
    const int x0 = (left + image.width()) % image.width();
    const int x1 = (left + 1) % image.width();
    const int y0 = (top + image.height()) % image.height();
    const int y1 = (top + 1) % image.height();
    const QRgb a = image.pixel(x0, y0);
    const QRgb b = image.pixel(x1, y0);
    const QRgb c = image.pixel(x0, y1);
    const QRgb d = image.pixel(x1, y1);
    const auto mix = [&](int av, int bv, int cv, int dv) {
        return qRound((av * (1 - dx) + bv * dx) * (1 - dy)
                    + (cv * (1 - dx) + dv * dx) * dy);
    };
    return qRgba(mix(qRed(a), qRed(b), qRed(c), qRed(d)),
                 mix(qGreen(a), qGreen(b), qGreen(c), qGreen(d)),
                 mix(qBlue(a), qBlue(b), qBlue(c), qBlue(d)),
                 mix(qAlpha(a), qAlpha(b), qAlpha(c), qAlpha(d)));
}

QImage bakePattern(const TraceMesh &mesh, const SourceUVs &uvs, const QImage &texture,
                   double scale, const QPointF &shift, QSize canvas)
{
    const QSize resolution = canvas.boundedTo(QSize(4000, 4000)) == canvas
        ? canvas : canvas.scaled(QSize(4000, 4000), Qt::KeepAspectRatio);
    QImage raster(resolution, QImage::Format_ARGB32);
    if (raster.isNull())
        return raster;

    const QImage source = texture.convertToFormat(QImage::Format_ARGB32);
    const double sx = double(raster.width()) / canvas.width();
    const double sy = double(raster.height()) / canvas.height();
    raster.fill(Qt::transparent);
    for (size_t i = 0; i < mesh.face.size(); ++i)
    {
        const auto &face = mesh.face[i];
        if (face.IsD() || !finiteUV(face.cWT(0).P())
            || !finiteUV(face.cWT(1).P()) || !finiteUV(face.cWT(2).P()))
            continue;

        const auto pixel = [&](int corner) {
            const QPointF p = patternPoint(face, corner, shift, scale);
            return QPointF(p.x() * sx, p.y() * sy);
        };
        const QPointF a = pixel(0), b = pixel(1), c = pixel(2);
        const double area = cross(b - a, c - a);
        if (!std::isfinite(area) || std::abs(area) < 1e-10)
            continue;

        const auto clip = [](double coordinate, int limit) {
            return int(std::max(0.0, std::min(double(limit), coordinate)));
        };
        const int x0 = clip(std::floor(std::min({a.x(), b.x(), c.x()})), raster.width());
        const int x1 = clip(std::ceil(std::max({a.x(), b.x(), c.x()})), raster.width());
        const int y0 = clip(std::floor(std::min({a.y(), b.y(), c.y()})), raster.height());
        const int y1 = clip(std::ceil(std::max({a.y(), b.y(), c.y()})), raster.height());
        for (int y = y0; y < y1; ++y)
        {
            auto *row = reinterpret_cast<QRgb *>(raster.scanLine(y));
            for (int x = x0; x < x1; ++x)
            {
                const QPointF local = QPointF(x + 0.5, y + 0.5) - a;
                const double wb = cross(local, c - a) / area;
                const double wc = cross(b - a, local) / area;
                const double wa = 1.0 - wb - wc;
                if (wa >= -1e-9 && wb >= -1e-9 && wc >= -1e-9)
                {
                    const auto uv = uvs[i][0] * wa + uvs[i][1] * wb + uvs[i][2] * wc;
                    row[x] = sampleTexture(source, uv.X(), uv.Y());
                }
            }
        }
    }
    return raster;
}

} // namespace

bool loadSourceUVs(const std::string &path, TraceMesh &mesh, SourceUVs &uvs)
{
    using namespace vcg::tri::io;
    const QString suffix = QFileInfo(QString::fromStdString(path)).suffix().toLower();
    int mask = 0;
    if (suffix == QStringLiteral("obj"))
        ImporterOBJ<TraceMesh>::LoadMask(path.c_str(), mask);
    else if (suffix == QStringLiteral("ply"))
        ImporterPLY<TraceMesh>::LoadMask(path.c_str(), mask);
    if (mesh.fn == 0 || !(mask & (Mask::IOM_WEDGTEXCOORD | Mask::IOM_VERTTEXCOORD)))
        return false;

    SourceUVs loaded(mesh.face.size());
    for (size_t i = 0; i < mesh.face.size(); ++i)
    {
        auto &face = mesh.face[i];
        for (int j = 0; j < 3; ++j)
        {
            if (!(mask & Mask::IOM_WEDGTEXCOORD))
                face.WT(j).P() = face.V(j)->T().P();
            loaded[i][j] = face.WT(j).P();
            if (!finiteUV(loaded[i][j]))
                return false;
        }
    }
    uvs.swap(loaded);
    return true;
}

SourceUVs projectSourceUVs(TraceMesh &source, const TraceMesh &target)
{
    using Scalar = TraceMesh::ScalarType;
    using Point = TraceMesh::CoordType;
    if (source.fn == 0 || target.fn == 0)
        return {};
    vcg::GridStaticPtr<TraceMesh::FaceType, Scalar> grid;
    grid.Set(source.face.begin(), source.face.end());
    const Scalar maxDistance = source.bbox.Diag() + target.bbox.Diag()
                             + (source.bbox.Center() - target.bbox.Center()).Norm();
    if (!(maxDistance > 0))
        return {};

    SourceUVs projected(target.face.size());
    for (size_t i = 0; i < target.face.size(); ++i)
    {
        const auto &targetFace = target.face[i];
        if (targetFace.IsD())
            continue;
        const Point center = (targetFace.cP(0) + targetFace.cP(1) + targetFace.cP(2)) / Scalar(3);
        for (int j = 0; j < 3; ++j)
        {
            const Point corner = targetFace.cP(j);
            // Query from inside the triangle so adjacent faces can keep different UVs at an atlas seam.
            const Point query = corner + (center - corner) * Scalar(0.001);
            Scalar distance;
            Point closest;
            const auto *face = vcg::tri::GetClosestFaceBase(source, grid, query, maxDistance,
                                                            distance, closest);
            Point barycentric;
            distance = maxDistance;
            if (!face || !vcg::face::PointDistanceBase(*face, corner, distance, closest)
                || !vcg::InterpolationParameters(*face, face->cN(), closest, barycentric))
                return {};
            projected[i][j] = face->cWT(0).P() * barycentric[0]
                            + face->cWT(1).P() * barycentric[1]
                            + face->cWT(2).P() * barycentric[2];
            if (!finiteUV(projected[i][j]))
                return {};
        }
    }
    return projected;
}

void drawTexturedPattern(TraceMesh &mesh, const SourceUVs &uvs, unsigned texture)
{
    const auto box = vcg::tri::UV_Utils<TraceMesh>::PerWedgeUVBox(mesh);
    if (!std::isfinite(box.Diag()) || box.Diag() <= 0)
        return;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    const double scale = 3.0 / box.Diag();
    glScaled(scale, scale, 1.0);
    glTranslated(-box.Center().X(), -box.Center().Y(), 0);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < mesh.face.size(); ++i)
    {
        const auto &face = mesh.face[i];
        if (face.IsD() || !finiteUV(face.cWT(0).P())
            || !finiteUV(face.cWT(1).P()) || !finiteUV(face.cWT(2).P()))
            continue;
        for (int j = 0; j < 3; ++j)
        {
            const auto &position = face.cWT(j).P();
            glTexCoord2d(uvs[i][j].X(), uvs[i][j].Y());
            glVertex3d(position.X(), position.Y(), 0);
        }
    }
    glEnd();
    glPopMatrix();
    glPopAttrib();
}

bool embedPatternTexture(const char *svgPath, const TraceMesh &mesh,
                         const SourceUVs &uvs, const QImage &texture,
                         double scale, QPointF shift, QSize canvas)
{
    if (texture.isNull() || uvs.empty() || uvs.size() != mesh.face.size() || canvas.isEmpty())
        return false;
    const QImage raster = bakePattern(mesh, uvs, texture, scale, shift, canvas);
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !raster.save(&buffer, "PNG"))
        return false;

    QFile outlines(QString::fromLocal8Bit(svgPath));
    if (!outlines.open(QIODevice::ReadOnly))
        return false;
    QByteArray svg = outlines.readAll();
    outlines.close();
    const int root = svg.indexOf("<svg");
    const int tagEnd = root < 0 ? -1 : svg.indexOf('>', root);
    if (tagEnd < 0)
        return false;

    const QByteArray layer = "\n<image x=\"0\" y=\"0\" width=\"" + QByteArray::number(canvas.width())
        + "\" height=\"" + QByteArray::number(canvas.height())
        + "\" preserveAspectRatio=\"none\" xlink:href=\"data:image/png;base64," + png.toBase64() + "\"/>\n";
    svg.insert(tagEnd + 1, layer);
    QSaveFile output(outlines.fileName());
    return output.open(QIODevice::WriteOnly) && output.write(svg) == svg.size() && output.commit();
}

} // namespace parafashion_tex
