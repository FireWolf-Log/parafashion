#pragma once

#include <array>
#include <string>
#include <vector>
#include <QImage>
#include <QPointF>
#include <vcg/space/point2.h>

class TraceMesh;

namespace parafashion_tex {

using FaceUVs = std::array<vcg::Point2d, 3>;
using SourceUVs = std::vector<FaceUVs>;

bool loadSourceUVs(const std::string &path, TraceMesh &mesh, SourceUVs &uvs);
SourceUVs projectSourceUVs(TraceMesh &source, const TraceMesh &target);
void drawTexturedPattern(TraceMesh &mesh, const SourceUVs &uvs, unsigned texture);
bool embedPatternTexture(const char *svgPath, const TraceMesh &mesh,
                         const SourceUVs &uvs, const QImage &texture,
                         double scale, QPointF shift, QSize canvas);

} // namespace parafashion_tex
