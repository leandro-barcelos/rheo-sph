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

    has_geo_transform_ =
        GDALGetGeoTransform(geotiff_dataset_, geo_transform_.data()) == CE_None;
  }
}

resources::GeoTiff::~GeoTiff() {
  if (geotiff_dataset_ != nullptr) {
    GDALClose(geotiff_dataset_);
  }
  GDALDestroyDriverManager();
}

std::vector<resources::Elevation> resources::GeoTiff::Elevations(
    int layer, float resolution_meters) { // NOLINT(bugprone-easily-swappable-parameters)
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
      const auto pixel_x = static_cast<double>(column_index);
      const auto pixel_y = static_cast<double>(row_index);

      const double origin_x = has_geo_transform_ ? geo_transform_[0] : 0.0;
      const double origin_z = has_geo_transform_ ? geo_transform_[3] : 0.0;
      const double world_x =
          origin_x + (pixel_x * static_cast<double>(resolution_meters));
      const double world_z =
          origin_z + (pixel_y * static_cast<double>(resolution_meters));
      const float elevation = buffer[(static_cast<size_t>(row_index) * width) +
                                     static_cast<size_t>(column_index)];
      elevations.push_back({.uv = {u_coord, v_coord},
                            .elevation = elevation,
                            .position = {static_cast<float>(world_x), elevation,
                                         static_cast<float>(world_z)}});
    }
  }

  return elevations;
}
