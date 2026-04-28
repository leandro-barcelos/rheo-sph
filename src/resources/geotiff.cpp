#include "geotiff.h"

#include <gdal.h>
#include <gdal_priv.h>

#include <algorithm>

resources::GeoTiff::GeoTiff(const char* filename)
    : filename_(filename),
      geotiff_dataset_(
          reinterpret_cast<GDALDataset*>(GDALOpen(filename_, GA_ReadOnly))) {
  GDALAllRegister();
  dimensions_ = {GDALGetRasterXSize(geotiff_dataset_),
                 GDALGetRasterYSize(geotiff_dataset_),
                 GDALGetRasterCount(geotiff_dataset_)};
}

resources::GeoTiff::~GeoTiff() {
  GDALClose(geotiff_dataset_);
  GDALDestroyDriverManager();
}

std::vector<std::vector<float>> resources::GeoTiff::Data(int layer) {
  if (geotiff_dataset_ == nullptr) {
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

  std::vector<std::vector<float>> out;
  out.reserve(height);
  for (int j = 0; j < height; ++j) {
    std::vector<float> row;
    row.resize(width);
    const float* src =
        buffer
            .data() +  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        (static_cast<size_t>(j) * width);
    for (int i = 0; i < width; ++i) {
      row[i] =
          src[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    out.push_back(std::move(row));
  }

  return out;
}
