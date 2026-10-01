#include <jacobi.h>
#include <math.h>

enum { SIZE = 4, MAX_SWEEPS = 64 };

static double offDiagonalNorm(double matrix[SIZE][SIZE]) {
  double sum = 0.0;
  int row, column;
  for (row = 0; row < SIZE; row++) {
    for (column = row + 1; column < SIZE; column++) {
      sum += matrix[row][column] * matrix[row][column];
    }
  }
  return sqrt(sum);
}

static void rotateColumns(double matrix[SIZE][SIZE], int p, int q, double c, double s) {
  int k;
  for (k = 0; k < SIZE; k++) {
    double kp = matrix[k][p], kq = matrix[k][q];
    matrix[k][p] = c * kp - s * kq;
    matrix[k][q] = s * kp + c * kq;
  }
}

static void rotateRows(double matrix[SIZE][SIZE], int p, int q, double c, double s) {
  int k;
  for (k = 0; k < SIZE; k++) {
    double pk = matrix[p][k], qk = matrix[q][k];
    matrix[p][k] = c * pk - s * qk;
    matrix[q][k] = s * pk + c * qk;
  }
}

static void annihilate(double matrix[SIZE][SIZE], double vectors[SIZE][SIZE], int p, int q) {
  double theta, t, c, s;
  if (matrix[p][q] == 0.0) {
    return;
  }
  theta = (matrix[q][q] - matrix[p][p]) / (2.0 * matrix[p][q]);
  t = (theta >= 0.0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1.0));
  c = 1.0 / sqrt(t * t + 1.0);
  s = t * c;
  rotateColumns(matrix, p, q, c, s);
  rotateRows(matrix, p, q, c, s);
  rotateColumns(vectors, p, q, c, s);
}

static void identity(double matrix[SIZE][SIZE]) {
  int row, column;
  for (row = 0; row < SIZE; row++) {
    for (column = 0; column < SIZE; column++) {
      matrix[row][column] = row == column ? 1.0 : 0.0;
    }
  }
}

void Jacobi_SymmetricEigen4(double matrix[4][4], double eigenvalues[4], double eigenvectors[4][4]) {
  double work[SIZE][SIZE];
  int sweep, p, q, row;
  for (row = 0; row < SIZE; row++) {
    for (p = 0; p < SIZE; p++) {
      work[row][p] = matrix[row][p];
    }
  }
  identity(eigenvectors);
  for (sweep = 0; sweep < MAX_SWEEPS && offDiagonalNorm(work) > 1e-14; sweep++) {
    for (p = 0; p < SIZE; p++) {
      for (q = p + 1; q < SIZE; q++) {
        annihilate(work, eigenvectors, p, q);
      }
    }
  }
  for (row = 0; row < SIZE; row++) {
    eigenvalues[row] = work[row][row];
  }
}
