#pragma once

#include <vector>
#include <cstddef>
#include <stdexcept>
#include <iostream>

namespace caf::cuda {

using dim_vec = std::vector<size_t>;

//class that represents the grid and block dimensions of a kernel
class nd_range {
public:
  // Constructor with individual dimension arguments
  nd_range(int gridX, int gridY, int gridZ, int blockX, int blockY, int blockZ)
      : gridDim{static_cast<size_t>(gridX),
                static_cast<size_t>(gridY),
                static_cast<size_t>(gridZ)},
        blockDim{static_cast<size_t>(blockX),
                 static_cast<size_t>(blockY),
                 static_cast<size_t>(blockZ)} {}


  // Constructor from vectors
  nd_range(const dim_vec& grid, const dim_vec& block) {
    if (grid.size() != 3 || block.size() != 3) {
      throw std::invalid_argument("Grid and block dimensions must each be of size 3.");
    }
    gridDim = grid;
    blockDim = block;
  }


  //default constructor
  //TODO fix issue where actors store a non in use ndrange
  nd_range() = default;


  // Getters for grid dimensions
  size_t getGridDimX() const { return gridDim[0]; }
  size_t getGridDimY() const { return gridDim[1]; }
  size_t getGridDimZ() const { return gridDim[2]; }

  // Getters for block dimensions
  size_t getBlockDimX() const { return blockDim[0]; }
  size_t getBlockDimY() const { return blockDim[1]; }
  size_t getBlockDimZ() const { return blockDim[2]; }

  // Optional: Getters for full vectors
  const dim_vec& getGridDims() const { return gridDim; }
  const dim_vec& getBlockDims() const { return blockDim; }

  ~nd_range() {
  //no-op
  }



private:
  // Dimensions are stored in order of x, y, z
  dim_vec gridDim{3};
  dim_vec blockDim{3};
};

} // namespace caf::cuda

