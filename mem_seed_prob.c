#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>


// MACROS //

#define LIBNAME "mem_seed_prob"
#define VERSION "1.0 05-25-2018"

#define YES 1
#define NO  0

#define SUCCESS 1
#define FAILURE 0

// Maximum allowed number of duplicates.
#define MAXN 1024

// Compute omega and tilde-omega.
#define  OMEGA ( P*pow(1.0-U/3, N) )
#define _OMEGA ( P*(1-pow(1.0-U/3, N)) )

// Prob that one of m altnerative threads survives i steps.
#define xi(i,m) ( 1.0 - pow( 1.0 - pow(1.0-U,(i)), (m) ))

// Calculation intermediates (one index).
#define aN(i) pow( 1.0 - pow(1.0-U,(i)) * U/3.0, N )
#define gN(i) pow( 1.0 - pow(1.0-U,(i)), N )
#define dN(i) pow( 1.0 - (1.0 - U + U*U/3.0) * pow(1.0-U,(i)), N )

// Calculation intermediates (two indices).
#define bN(j,i) pow( 1.0 - pow(1.0-U,(j))*U/3.0 - \
                           pow(1.0-U,(i))*(1.0-U/3.0), N)

#define zN(j,i) pow( 1.0 - pow(1.0-U,(j))*U/3.0 - \
                           pow(1.0-U,(i))*U/3.0*(1.0-U/3.0), N)


// Macro to simplify error handling.
#define handle_memory_error(x) do { \
   if ((x) == NULL) { \
      warning("memory_error", __func__, __LINE__); \
      ERRNO = __LINE__; \
      goto in_case_of_failure; \
}} while (0)


//  TYPE DECLARATIONS  //

typedef struct trunc_pol_t  trunc_pol_t;
typedef struct matrix_t     matrix_t;
typedef struct monomial_t   monomial_t;

// TODO : explain that monomials must not be just deg and coeff //

struct monomial_t {
   size_t deg;           // Degree of the coefficent.
   double coeff;         // Value of the coefficient
};

struct trunc_pol_t {
   monomial_t mono;      // Monomial (if applicable).
   double coeff[];       // Terms of the polynomial.
};

struct matrix_t {
   const size_t    dim;  // Column / row number.
   trunc_pol_t * term[]; // Terms of the matrix.
};



// GLOBAL VARIABLES / CONSTANTS //

static size_t    G = 0;       // Minimum size of MEM seeds.
static size_t    K = 0;       // Max degree (read size).

static double    P = 0.0;     // Probability of a read error.
static double    U = 0.0;     // Divergence rate between duplicates.

static size_t    KSZ = 0;     // Size of the 'trunc_pol_t' struct.

static trunc_pol_t * TEMP = NULL;        // For matrix multipliciation.
static trunc_pol_t * ARRAY[MAXN] = {0};  // Store results indexed by N.

static int MAX_PRECISION = 0; // Full precision for debugging.
static int ERRNO = 0;

// Error message.
const char internal_error[] =
   "internal error (please contact guillaume.filion@gmail.com)";



// FUNCTION DEFINITIONS //

// IO and error report functions (all VISIBLE).
int  get_mem_prob_error_code (void) { return ERRNO; }
void reset_mem_prob_error (void) { ERRNO = 0; }
void set_mem_prob_max_precision_on (void) { MAX_PRECISION = 1; }
void set_mem_prob_max_precision_off (void) { MAX_PRECISION = 0; }


void
warning
(
   const char * msg,
   const char * function,
   const int    line
)
// Print warning message to stderr.
{
   fprintf(stderr, "[%s] error in function `%s' (line %d): %s\n",
         LIBNAME, function, line, msg);
}


// Initialization and clean up.

void
clean_mem_prob // VISIBLE //
(void)
{

   // Set global variables to "fail" values.
   G = 0;
   K = 0;
   P = 0.0;
   U = 0.0;
   KSZ = 0;

   // Free everything.
   free(TEMP);
   TEMP = NULL;

   for (int i = 0 ; i < MAXN ; i++) free(ARRAY[i]);
   bzero(ARRAY, MAXN * sizeof(trunc_pol_t *));

   MAX_PRECISION = 0;
   ERRNO = 0;

   return;

}


int
set_params_mem_prob // VISIBLE //
(
   size_t g,
   size_t k,
   double p,
   double u
)
// Initialize the global variables from user-defined values.
{

   // Check input
   if (g == 0 || k == 0) {
      ERRNO = __LINE__;
      warning("parameters g and k must greater than 0",
            __func__, __LINE__); 
      goto in_case_of_failure;
   }

   if (k < g) {
      ERRNO = __LINE__;
      warning("parameters k must be greater than g",
            __func__, __LINE__); 
      goto in_case_of_failure;
   }

   if (p <= 0.0 || p >= 1.0) {
      ERRNO = __LINE__;
      warning("parameter p must be between 0 and 1",
            __func__, __LINE__); 
      goto in_case_of_failure;
   }

   if (u <= 0.0 || u >= 1.0) {
      ERRNO = __LINE__;
      warning("parameter u must be between 0 and 1",
            __func__, __LINE__); 
      goto in_case_of_failure;
   }

   G = g;  // MEM size.
   K = k;  // Read size.
   P = p;  // Sequencing error.
   U = u;  // Divergence rate.

   // All 'trunc_pol_t' must be congruent.
   KSZ = sizeof(trunc_pol_t) + (K+1) * sizeof(double);

   // Clean previous values (if any).
   for (int i = 0 ; i < MAXN ; i++) free(ARRAY[i]);
   bzero(ARRAY, MAXN * sizeof(trunc_pol_t *));

   // Allocate or reallocate 'TEMP'.
   free(TEMP);
   TEMP = calloc(1, KSZ);
   handle_memory_error(TEMP);

   return SUCCESS;

in_case_of_failure:
   clean_mem_prob();
   return FAILURE;

}



// Definitions of weighted generating functions.

trunc_pol_t *
new_zero_trunc_pol
(void)
// IMPORTANT: do not call before set_params_mem_prob().
{

   if (G == 0 || K == 0 || P == 0 || U == 0 || KSZ == 0) {
      warning("parameters unset: call `set_params_mem_prob'",
            __func__, __LINE__);
      goto in_case_of_failure;
   }

   trunc_pol_t *new = calloc(1, KSZ);
   handle_memory_error(new);

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_A_ddown
(
   const size_t N     // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   if (N == 0) {
      // In the special case N = 0, A_ddown(z) = pz.
      new->mono.deg = 1;
      new->mono.coeff = P;
      new->coeff[1] = P;
      return new;
   }

   // See definition of polynomial A_ddown.
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= G ; i++) {
      new->coeff[i] = OMEGA * (xi(i-1,N)) * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_A_down
(
   const size_t N     // Number of duplicates.
)
{
   
   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   if (N == 0) return new;

   // See definition of polynomial A_down.
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= G ; i++) {
      new->coeff[i] = _OMEGA * (xi(i-1,N)) * pow_of_q;
      pow_of_q *= (1.0-P);
   }
   // Terms of the polynomials with degree higher than 'G' (if any).
   for (int i = G+1 ; i <= K ; i++) {
      new->coeff[i] = P * (1-aN(i-1)) * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_B_ddown
(
   const size_t N     // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // Polynomial B_ddown is null when N = 0.
   if (N == 0) return new;

   // See definition of polynomial B_ddown.
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= G ; i++) {
      new->coeff[i] = OMEGA * xi(i-1,N) * pow_of_q;
      pow_of_q *= (1.0-P);
   }
   // Terms of the polynomials with degree higher than 'G'.
   const double denom = 1.0 - pow(1-U/3.0, N);
   for (int i = G+1 ; i <= K ; i++) {
      new->coeff[i] = OMEGA * (1-aN(i-1)) * pow_of_q / denom;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_B_down
(
   const size_t N     // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // Polynomial B_down is null when N = 0.
   if (N == 0) return new;

   // See definition of polynomial B_down.
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= G ; i++) {
      new->coeff[i] = _OMEGA * xi(i-1,N) * pow_of_q;
      pow_of_q *= (1.0-P);
   }
   const double denom = 1.0 - pow(1-U/3.0, N);
   for (int i = G+1 ; i <= K ; i++) {
      new->coeff[i] = P * pow_of_q * (1.0 - aN(i-1) +
         (aN(i-1)-aN(0)-zN(i-1,i-1)+zN(0,i-1)) / denom);
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_C_ddown
(
   const size_t deg,  // Degree of polynomial D.
   const size_t N     // Number of duplicates.
)
{

   if (deg > K || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   // NB: The special case N = 0 is implicit for the
   // polynomial D_ddown (it implies omega = p).
   
   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial D.
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= deg ; i++) {
      new->coeff[i] = OMEGA * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_C_down
(
   const size_t deg,  // Degree of polynomial D.
   const size_t N     // Number of duplicates.
)
{

   if (deg > K || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   if (N == 0) return new;

   // See definition of polynomial D_down
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= deg ; i++) {
      new->coeff[i] = _OMEGA * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_u
(
   const size_t deg,  // Degree of polynomial u.
   const size_t N     // Number of duplicates.
)
{

   if (deg > K || deg >= G || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial u.
   new->mono.deg = deg;
   // The case N = 0 involves the term 'xi(0,0)', which should be
   // equal to 0 but is equal to 1 because 'pow(0,0)' is 1 as per
   // IEEE 854. So we need to return a special value for this case.
   new->mono.coeff = (N == 0 && deg == 1) ? 1.0-P : 
      (xi(deg-1,N) - xi(deg,N)) * pow(1.0-P,deg);
   new->coeff[deg] = new->mono.coeff;

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_down
(
   const size_t N    // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // In the special case N = 0, the polynomial T is
   // undefined, but we return a zero polynomial.
   if (N == 0) return new;

   // See definition of polynomial T_down.
   new->coeff[0] = 1.0;
   double pow_of_q = 1.0-P;
   for (int i = 1 ; i <= G-1 ; i++) {
      new->coeff[i] = xi(i,N) * pow_of_q;
      pow_of_q *= (1.0-P);
   }
   const double denom = 1.0 - pow(1-U/3.0, N);
   for (int i = G ; i <= K ; i++) {
      new->coeff[i] = pow_of_q * (1.0 - aN(i)) / denom;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_ddown
(
   const size_t N    // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   if (N == 0) {
      // Special case N = 0.
      new->mono.coeff = 1.0;
      new->coeff[new->mono.deg] = new->mono.coeff;
   }
   else {
      double pow_of_q = 1.0;
      for (int i = 0 ; i <= G-1 ; i++) {
         new->coeff[i] = xi(i,N) * pow_of_q;
         pow_of_q *= (1-P);
      }
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_up
(
   size_t deg,       // Degree of the polynomial.
   const size_t N    // Number of duplicates.
)
{

   if (deg > K || deg >= G) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // The special case N = 0 is implicit.

   double pow_of_q = 1.0;
   for (int i = 0 ; i <= deg ; i++) {
      new->coeff[i] = pow_of_q;
      pow_of_q *= (1-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


matrix_t *
new_null_matrix
(
   const size_t dim
)
// Create a matrix where all truncated polynomials
// (trunc_pol_t) are set NULL.
{

   // Initialize to zero.
   size_t sz = sizeof(matrix_t) + dim*dim * sizeof(trunc_pol_t *);
   matrix_t *new = calloc(1, sz);
   handle_memory_error(new);

   // The dimension is set upon creation
   // and must never change afterwards.
   *(size_t *)&new->dim = dim;

   return new;

in_case_of_failure:
   return NULL;

}


void
destroy_mat
(
   matrix_t * mat
)
{

   // Do not try anything on NULL.
   if (mat == NULL) return;

   size_t nterms = (mat->dim)*(mat->dim);
   for (size_t i = 0 ; i < nterms ; i++)
      free(mat->term[i]);
   free(mat);

}



matrix_t *
new_zero_matrix
(
   const size_t dim
)
// Create a matrix where all terms
// are 'trunc_pol_t' struct set to zero.
{

   matrix_t *new = new_null_matrix(dim);
   handle_memory_error(new);

   for (int i = 0 ; i < dim*dim ; i++) {
      new->term[i] = new_zero_trunc_pol();
      handle_memory_error(new->term[i]);
   }

   return new;

in_case_of_failure:
   destroy_mat(new);
   return NULL;

}



matrix_t *
new_matrix_M
(
   const size_t N    // Number of duplicates.
)
{

   const size_t dim = G+2;
   matrix_t *M = new_null_matrix(dim);
   handle_memory_error(M);

   // First row is null.

   // Second row.
   M->term[1*dim+1] = new_trunc_pol_A_ddown(N);
   handle_memory_error(M->term[1*dim+1]);
   M->term[1*dim+2] = new_trunc_pol_A_down(N);
   handle_memory_error(M->term[1*dim+2]);
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[1*dim+(j+2)] = new_trunc_pol_u(j, N);
      handle_memory_error(M->term[1*dim+(j+2)]);
   }
   M->term[1*dim+0] = new_trunc_pol_T_ddown(N);
   handle_memory_error(M->term[1*dim+0]);

   // Third row.
   M->term[2*dim+1] = new_trunc_pol_B_ddown(N);
   handle_memory_error(M->term[2*dim+1]);
   M->term[2*dim+2] = new_trunc_pol_B_down(N);
   handle_memory_error(M->term[2*dim+2]);
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[2*dim+(j+2)] = new_trunc_pol_u(j, N);
      handle_memory_error(M->term[2*dim+j+2]);
   }
   M->term[2*dim+0] = new_trunc_pol_T_down(N);
   handle_memory_error(M->term[2*dim+0]);

   // Middle rows.
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[(j+2)*dim+1] = new_trunc_pol_C_ddown(G-j, N);
      handle_memory_error(M->term[(j+2)*dim+1]);
      M->term[(j+2)*dim+2] = new_trunc_pol_C_down(G-j, N);
      handle_memory_error(M->term[(j+2)*dim+2]);
      M->term[(j+2)*dim+0] = new_trunc_pol_T_up(G-j-1, N);
      handle_memory_error(M->term[(j+2)*dim+0]);
   }

   return M;

in_case_of_failure:
   destroy_mat(M);
   return NULL;

}


trunc_pol_t *
trunc_pol_mult
(
         trunc_pol_t * dest,
   const trunc_pol_t * a,
   const trunc_pol_t * b
)
{

   // Erase destination.
   bzero(dest, KSZ);

   // If any of the two k-polynomials is zero,
   // set 'dest' to zero and return 'NULL' (not dest).
   if (a == NULL || b == NULL) return NULL;

   if (a->mono.coeff && b->mono.coeff) {
      // Both are monomials, just do one multiplication.
      // If degree is too high, all coefficients are zero.
      if (a->mono.deg + b->mono.deg > K) return NULL;
      // Otherwise do the multiplication.
      dest->mono.deg = a->mono.deg + b->mono.deg;
      dest->mono.coeff = a->mono.coeff * b->mono.coeff;
      dest->coeff[dest->mono.deg] = dest->mono.coeff;
   }
   else if (a->mono.coeff) {
      // 'a' is a monomial, do one "row" of multiplications.
      for (int i = a->mono.deg ; i <= K ; i++)
         dest->coeff[i] = a->mono.coeff * b->coeff[i-a->mono.deg];
   }
   else if (b->mono.coeff) {
      // 'b' is a monomial, do one "row" of multiplications.
      for (int i = b->mono.deg ; i <= K ; i++)
         dest->coeff[i] = b->mono.coeff * a->coeff[i-b->mono.deg];
   }
   else {
      // Standard convolution product.
      for (int i = 0 ; i <= K ; i++) {
         dest->coeff[i] = a->coeff[0] * b->coeff[i];
         for (int j = 1 ; j <= i ; j++) {
            dest->coeff[i] += a->coeff[j] * b->coeff[i-j];
         }
      }
   }

   return dest;

}


void
trunc_pol_update_add
(
         trunc_pol_t * dest,
   const trunc_pol_t * a
)
{

   // No update when adding zero.
   if (a == NULL) return;

   // No monomial after update.
   dest->mono = (monomial_t) {0};

   for (int i = 0 ; i <= K ; i++) {
      dest->coeff[i] += a->coeff[i];
   }

}


matrix_t *
special_matrix_mult
(
         matrix_t * dest,
   const matrix_t * a,
   const matrix_t * b
)
// This matrix multipication is "special" because it ignores all
// the terms on the right of the first NULL value for each row of
// the left matrix. This is done for optimization purposes and the
// matrices used here are designed in such a way that the result
// is correct, but in general, the code cannot be used for other
// applications.
{

   if (a->dim != dest->dim || b->dim != dest->dim) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   size_t dim = dest->dim;

   for (int i = 0 ; i < dim ; i++) {
   for (int j = 0 ; j < dim ; j++) {
      // Erase destination entry.
      bzero(dest->term[i*dim+j], KSZ);
      for (int m = 0 ; m < dim ; m++) {
         // By construction, the first NULL term of the row
         // indicates that all the remaining terms are zero.
         if (a->term[i*dim+m] == NULL) break;
         trunc_pol_update_add(dest->term[i*dim+j],
            trunc_pol_mult(TEMP, a->term[i*dim+m], b->term[m*dim+j]));
      }
   }
   }

   return dest;

in_case_of_failure:
   return NULL;

}



// Need this snippet to compute a bound of the numerical imprecision.
double HH(double x, double y) { return x*log(x/y)+(1-x)*log((1-x)/(1-y)); }

double
mem_seed_prob // VISIBLE //
(
   const size_t N,    // Number of duplicates.
   const size_t k     // Segment or read size.
)
{
   
   // Those variables must be declared here so that
   // they can be cleaned in case of failure.
   matrix_t *          M = NULL;  // Transfer matix.
   matrix_t *      powM1 = NULL;  // Computation intermediate.
   matrix_t *      powM2 = NULL;  // Computation intermediate.
   trunc_pol_t *       w = NULL;  // Result.

   // Check if sequencing parameters were set. Otherwise
   // warn user and fail gracefully (return nan).
   if (G == 0 || K == 0 || P == 0 || U == 0 || KSZ == 0) {
      warning("parameters unset: call `set_params_mem_prob'",
            __func__, __LINE__);
      goto in_case_of_failure;
   }

   // Check input.
   if (N > MAXN-1) {
      char msg[128];
      snprintf(msg, 128, "argument N greater than %d", MAXN);
      warning(msg, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   if (k > K) {
      char msg[128];
      snprintf(msg, 128, "argument k greater than set value (%ld)", K);
      warning(msg, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }


   // If results were not computed for given number of duplicates (N),
   // need to compute truncated genearting function and store the
   // results for future use.
   if (ARRAY[N] == NULL) {
      
      // The truncated polynomial 'w' stands the weighted generating
      // function of the reads without MEM seed for set parameters and
      // 'N' duplicate sequences.
      w = new_zero_trunc_pol();
      handle_memory_error(w);

      // The matrix 'M' is the transfer matrix of reads without MEM
      // seed. The row and column of the head state have been deleted
      // because the reads can be assumed to start in state "double-
      // down". The entries of 'M' are truncated polynomials that
      // stand for weighted generating functions of segments joining
      // different states. The m-th power of 'M' contains the weighted
      // generating functions of sequences of m segments joining the
      // dfferent states. We thus update 'w' with the entry of M^m
      // that joins the initial "double-down" state to the tail state.
      M = new_matrix_M(N);
      handle_memory_error(M);

      // Update weighted generating function with
      // one-segment reads (i.e. tail only).
      trunc_pol_update_add(w, M->term[G+2]);

      // Create two temporary matrices 'powM1' and 'powM2' to compute
      // the powers of M. On first iteration, powM1 = M*M, and later
      // perform the operations powM2 = M*powM1 and powM1 = M*powM2,
      // so that powM1 is M^2m at iteration m. Using two matrices
      // allows the operations to be performed without requesting more
      // memory. The temporary matrices are implicitly erased in the
      // course of the multiplication by 'special_matrix_mult()'.
      powM1 = new_zero_matrix(G+2);
      powM2 = new_zero_matrix(G+2);
      handle_memory_error(powM1);
      handle_memory_error(powM2);

      // Note: the matrix M, containing NULL entries, must always
      // be put on the left, i.e. as the second argument of 
      // 'special_matrix_mult()', otherwise the result is not
      // guaranteed to be correct.
      special_matrix_mult(powM1, M, M);

      // Update weighted generating function with two-segment reads.
      trunc_pol_update_add(w, powM1->term[G+2]);

      // There is at least one sequencing error for every three
      // segments. We bound the probability that a read of size k has
      // at least m/3 errors by a formula for the binomial distribution,
      // where m is the number of segments, i.e. the power of matirx M.
      // https://en.wikipedia.org/wiki/Binomial_distribution#Tail_Bounds
      for (int m = 2 ; m < K ; m += 2) {
         // Increase the number of segments and update
         // the weighted generating function accordingly.
         special_matrix_mult(powM2, M, powM1);
         trunc_pol_update_add(w, powM2->term[G+2]);
         special_matrix_mult(powM1, M, powM2);
         trunc_pol_update_add(w, powM1->term[G+2]);
         // In max precision debug mode, get all possible digits.
         if (MAX_PRECISION) continue;
         // Otherwise, stop when reaching 1% precision.
         double x = floor((m+2)/3) / ((double) K);
         double bound_on_imprecision = exp(-HH(x, P)*K);
         if (bound_on_imprecision / w->coeff[K] < 1e-2) break;
      }

      // Clean temporary variables.
      destroy_mat(powM1);
      destroy_mat(powM2);
      destroy_mat(M);

      // Memoize the results for future calls.
      ARRAY[N] = w;

   }

   return ARRAY[N]->coeff[k];

in_case_of_failure:
   // Clean everything.
   free(w);
   destroy_mat(powM1);
   destroy_mat(powM2);
   destroy_mat(M);
   return 0.0/0.0;  //  nan

}