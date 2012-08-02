#ifndef __GLOBAL_H__
#define __GLOBAL_H__

namespace sirius {

class global_fft : public sirius_geometry
{
    protected:
        
        /// plane wave cutoff radius (in inverse a.u. of length)
        double pw_cutoff_;
        
        /// fft wrapper
        FFT3D fft_;

        /// list of G-vector fractional coordinates
        mdarray<int,2> gvec_;

        /// number of G-vectors within plane wave cutoff
        int num_gvec_;

        /// mapping between linear G-vector index and G-vector coordinates
        mdarray<int,3> index_by_gvec_;

        /// mapping betwee linear G-vector index and position in FFT buffer
        std::vector<int> fft_index_;

    public:
    
        global_fft() : pw_cutoff_(pw_cutoff_default),
                       num_gvec_(0)
        {
        }
  
        void set_pw_cutoff(double _pw_cutoff)
        {
            pw_cutoff_ = _pw_cutoff;
        }

        void init_fft_grid()
        {
            Timer t("init_fft_grid");
            
            int max_frac_coord[] = {0, 0, 0};
            double frac_coord[3];
            // try three directions
            for (int i = 0; i < 3; i++)
            {
                double cart_coord[] = {0.0, 0.0, 0.0};
                cart_coord[i] = pw_cutoff_;
                get_reciprocal_fractional_coordinates(cart_coord, frac_coord);
                for (int i = 0; i < 3; i++)
                    max_frac_coord[i] = std::max(max_frac_coord[i], 2 * abs(int(frac_coord[i])) + 1);
            }
            
            fft_.init(max_frac_coord);
            
            mdarray<int,2> gvec(NULL, 3, fft_.size());
            gvec.allocate();
            std::vector<double> length(fft_.size());

            int ig = 0;
            for (int i = fft_.grid_limits(0, 0); i <= fft_.grid_limits(0, 1); i++)
                for (int j = fft_.grid_limits(1, 0); j <= fft_.grid_limits(1, 1); j++)
                    for (int k = fft_.grid_limits(2, 0); k <= fft_.grid_limits(2, 1); k++)
                    {
                        gvec(0, ig) = i;
                        gvec(1, ig) = j;
                        gvec(2, ig) = k;

                        int fracc[] = {i, j, k};
                        double cartc[3];
                        get_reciprocal_cartesian_coordinates(fracc, cartc);
                        length[ig] = vector_length(cartc);
                        ig++;
                    }

            std::vector<size_t> reorder(fft_.size());
            gsl_heapsort_index(&reorder[0], &length[0], fft_.size(), sizeof(double), compare_doubles);
           
            gvec_.set_dimensions(3, fft_.size());
            gvec_.allocate();

            num_gvec_ = 0;
            for (int i = 0; i < fft_.size(); i++)
            {
                for (int j = 0; j < 3; j++)
                    gvec_(j, i) = gvec(j, reorder[i]);
                
                if (length[reorder[i]] <= pw_cutoff_)
                    num_gvec_++;
            }

            index_by_gvec_.set_dimensions(dimension(fft_.grid_limits(0, 0), fft_.grid_limits(0, 1)),
                                          dimension(fft_.grid_limits(1, 0), fft_.grid_limits(1, 1)),
                                          dimension(fft_.grid_limits(2, 0), fft_.grid_limits(2, 1)));
            index_by_gvec_.allocate();

            fft_index_.resize(fft_.size());

            for (int ig = 0; ig < fft_.size(); ig++)
            {
                int i0 = gvec_(0, ig);
                int i1 = gvec_(1, ig);
                int i2 = gvec_(2, ig);

                // mapping from G-vector to it's index
                index_by_gvec_(i0, i1, i2) = ig;

                // mapping of FFT buffer linear index
                fft_index_[ig] = fft_.index(i0, i1, i2);
           }
        }
        
        inline FFT3D& fft()
        {
            return fft_;
        }

        inline int index_by_gvec(int i0, int i1, int i2)
        {
            return index_by_gvec_(i0, i1, i2);
        }

        inline int fft_index(int ig)
        {
            return fft_index_[ig];
        }
 

};

class Global : public sirius_gvec
{
    public:
    
        void initialize()
        {
            get_symmetry();
            init_fft_grid();
            find_nearest_neighbours();
            find_mt_radii();
        }

        void print_info()
        {
            printf("\n");
            printf("SIRIUS v0.1\n");
            printf("\n");

            printf("lattice vectors\n");
            for (int i = 0; i < 3; i++)
                printf("  a%1i : %18.10f %18.10f %18.10f \n", i + 1, lattice_vectors_[i][0], 
                                                                     lattice_vectors_[i][1], 
                                                                     lattice_vectors_[i][2]); 
            printf("reciprocal lattice vectors\n");
            for (int i = 0; i < 3; i++)
                printf("  b%1i : %18.10f %18.10f %18.10f \n", i + 1, reciprocal_lattice_vectors_[i][0], 
                                                                     reciprocal_lattice_vectors_[i][1], 
                                                                     reciprocal_lattice_vectors_[i][2]);
            std::map<int,AtomType*>::iterator it;    
            printf("\n"); 
            printf("number of atom types : %i\n", (int)atom_type_by_id_.size());
            for (it = atom_type_by_id_.begin(); it != atom_type_by_id_.end(); it++)
                printf("type id : %i   symbol : %s   label : %s   mt_radius : %f\n", (*it).first, 
                                                                                     (*it).second->symbol().c_str(), 
                                                                                     (*it).second->label().c_str(),
                                                                                     (*it).second->mt_radius()); 
                
            printf("number of atoms : %i\n", (int)atoms_.size());
            printf("number of symmetry classes : %i\n", (int)atom_symmetry_class_by_id_.size());

            printf("\n"); 
            printf("atom id    type id    class id\n");
            printf("------------------------------\n");
            for (int i = 0; i < (int)atoms_.size(); i++)
            {
                printf("%6i     %6i      %6i\n", i, atoms_[i]->type_id(), atoms_[i]->symmetry_class_id()); 
           
            }

            printf("\n");
            for (int ic = 0; ic < (int)atom_symmetry_class_by_id_.size(); ic++)
            {
                printf("class id : %i   atom id : ", ic);
                for (int i = 0; i < atom_symmetry_class_by_id_[ic]->num_atoms(); i++)
                    printf("%i ", atom_symmetry_class_by_id_[ic]->atom_id(i));  
                printf("\n");
            }

            printf("\n");
            printf("space group number   : %i\n", spg_dataset_->spacegroup_number);
            printf("international symbol : %s\n", spg_dataset_->international_symbol);
            printf("Hall symbol          : %s\n", spg_dataset_->hall_symbol);
            printf("number of operations : %i\n", spg_dataset_->n_operations);
            
            printf("\n");
            printf("plane wave cutoff : %f\n", pw_cutoff_);
            printf("FFT grid size : %i %i %i   total : %i\n", fft_.size(0), fft_.size(1), fft_.size(2), fft_.size());
            
            printf("\n");
            Timer::print();
        }
};

};

sirius::Global sirius_global;

#endif // __GLOBAL_H__
