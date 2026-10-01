#include <cest>
#include <cmath>
extern "C" {
#include <jacobi.h>
}

static void expectEigenPairs(double matrix[4][4], double values[4], double vectors[4][4]) {
  for (int pair = 0; pair < 4; pair++) {
    for (int row = 0; row < 4; row++) {
      double product = 0.0;
      for (int k = 0; k < 4; k++) {
        product += matrix[row][k] * vectors[k][pair];
      }
      expect(product).toBe(values[pair] * vectors[row][pair], 1e-9);
    }
  }
}

describe("Jacobi", []() {
  it("returns a diagonal matrix unchanged", []() {
    double matrix[4][4] = {{4, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, 2, 0}, {0, 0, 0, 7}};
    double values[4], vectors[4][4];
    Jacobi_SymmetricEigen4(matrix, values, vectors);
    expect(values[0]).toBe(4.0);
    expect(values[3]).toBe(7.0);
    expect(vectors[1][1]).toBe(1.0);
  });

  it("diagonalises a dense symmetric matrix", []() {
    double matrix[4][4] = {{4, 1, -2, 2}, {1, 2, 0, 1}, {-2, 0, 3, -2}, {2, 1, -2, -1}};
    double values[4], vectors[4][4];
    Jacobi_SymmetricEigen4(matrix, values, vectors);
    expectEigenPairs(matrix, values, vectors);
    expect(values[0] + values[1] + values[2] + values[3]).toBe(8.0, 1e-9);
  });

  it("returns orthonormal eigenvectors", []() {
    double matrix[4][4] = {{1, 2, 3, 4}, {2, 5, 6, 7}, {3, 6, 8, 9}, {4, 7, 9, 10}};
    double values[4], vectors[4][4];
    Jacobi_SymmetricEigen4(matrix, values, vectors);
    for (int a = 0; a < 4; a++) {
      for (int b = 0; b < 4; b++) {
        double dot = 0.0;
        for (int k = 0; k < 4; k++) {
          dot += vectors[k][a] * vectors[k][b];
        }
        expect(dot).toBe(a == b ? 1.0 : 0.0, 1e-9);
      }
    }
  });
});
