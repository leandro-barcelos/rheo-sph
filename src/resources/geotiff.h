#ifndef RHEOSPH_GEOTIFF_H
#define RHEOSPH_GEOTIFF_H

#include <gdal.h>
#include <gdal_dataset.h>

#include <array>
#include <vector>

#include "elevation.h"

namespace resources {

class GeoTiff {
 public:
  GeoTiff(const GeoTiff&) = default;
  GeoTiff(GeoTiff&&) = delete;
  GeoTiff& operator=(const GeoTiff&) = default;
  GeoTiff& operator=(GeoTiff&&) = delete;
  explicit GeoTiff(const char* filename);
  ~GeoTiff();

  [[nodiscard]] std::array<int, 3> const& Dimensions() const {
    return dimensions_;
  }
  [[nodiscard]] std::vector<Elevation> Elevations(
      int layer = 1, float resolution_meters = 10.0F);

 private:
  const char* filename_;
  GDALDataset* geotiff_dataset_ = nullptr;
  std::array<int, 3> dimensions_{0};
  std::array<double, 6> geo_transform_{0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  bool has_geo_transform_ = false;
};

}  // namespace resources

#endif  // !RHEOSPH_GEOTIFF_H
