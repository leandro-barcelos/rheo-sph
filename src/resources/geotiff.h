#ifndef RHEOSPH_GEOTIFF_H
#define RHEOSPH_GEOTIFF_H

#include <gdal.h>
#include <gdal_dataset.h>

#include <array>

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
  [[nodiscard]] std::vector<std::vector<float>> Data(int layer = 1);

 private:
  const char* filename_;
  GDALDataset* geotiff_dataset_ = nullptr;
  std::array<int, 3> dimensions_{0};
};

}  // namespace resources

#endif  // !RHEOSPH_GEOTIFF_H
