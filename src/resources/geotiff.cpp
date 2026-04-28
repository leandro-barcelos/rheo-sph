#include "geotiff.h"

#include <gdal.h>
#include <gdal_priv.h>

#include <algorithm>

namespace {

GDALDataset* OpenGeoTiffDataset(char const* filename) {
  GDALAllRegister();
  return reinterpret_cast<GDALDataset*>(GDALOpen(filename, GA_ReadOnly));
}

}  // namespace

resources::GeoTiff::GeoTiff(const char* filename)
    : filename_(filename), geotiff_dataset_(OpenGeoTiffDataset(filename_)) {
  if (geotiff_dataset_ != nullptr) {
    dimensions_ = {GDALGetRasterXSize(geotiff_dataset_),
                   GDALGetRasterYSize(geotiff_dataset_),
                   GDALGetRasterCount(geotiff_dataset_)};
  }
}

resources::GeoTiff::~GeoTiff() {
  if (geotiff_dataset_ != nullptr) {
    GDALClose(geotiff_dataset_);
  }
  GDALDestroyDriverManager();
}

std::vector<resources::Elevation> resources::GeoTiff::Elevations(int layer) {
  if (geotiff_dataset_ == nullptr) {
    return {};
  }

  if (dimensions_[0] <= 0 || dimensions_[1] <= 0 || dimensions_[2] <= 0) {
    return {};
  }

  layer = std::max(layer, 1);
  layer = std::min(layer, dimensions_[2]);

  const int width = dimensions_[0];
  const int height = dimensions_[1];

  GDALRasterBand* band = geotiff_dataset_->GetRasterBand(layer);
  if (band == nullptr) {
    return {};
  }

  std::vector<float> buffer(static_cast<size_t>(width) * height);
  CPLErr err = band->RasterIO(GF_Read, 0, 0, width, height, buffer.data(),
                              width, height, GDT_Float32, 0, 0);
  if (err != CE_None) {
    return {};
  }

  std::vector<resources::Elevation> elevations;
  elevations.reserve(static_cast<size_t>(width) * static_cast<size_t>(height));

  for (int row_index = 0; row_index < height; ++row_index) {
    for (int column_index = 0; column_index < width; ++column_index) {
      const float width_denominator =
          width > 1 ? static_cast<float>(width - 1) : 1.0F;
      const float height_denominator =
          height > 1 ? static_cast<float>(height - 1) : 1.0F;
      const float u_coord =
          static_cast<float>(column_index) / width_denominator;
      const float v_coord = static_cast<float>(row_index) / height_denominator;
      elevations.push_back(
          {.uv = {u_coord, v_coord},
           .elevation = buffer[(static_cast<size_t>(row_index) * width) +
                               static_cast<size_t>(column_index)]});
    }
  }

  return elevations;
}
