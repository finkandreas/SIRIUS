inline void K_point::generate_gkvec(double gk_cutoff)
{
    PROFILE();

    if (ctx_.full_potential() && (gk_cutoff * unit_cell_.max_mt_radius() > ctx_.lmax_apw())) {
        std::stringstream s;
        s << "G+k cutoff (" << gk_cutoff << ") is too large for a given lmax (" 
          << ctx_.lmax_apw() << ") and a maximum MT radius (" << unit_cell_.max_mt_radius() << ")" << std::endl
          << "suggested minimum value for lmax : " << int(gk_cutoff * unit_cell_.max_mt_radius()) + 1;
        WARNING(s);
    }

    if (gk_cutoff * 2 > ctx_.pw_cutoff()) {
        std::stringstream s;
        s << "G+k cutoff is too large for a given plane-wave cutoff" << std::endl
          << "  pw cutoff : " << ctx_.pw_cutoff() << std::endl
          << "  doubled G+k cutoff : " << gk_cutoff * 2;
        TERMINATE(s);
    }

    /* create G+k vectors */
    if (ctx_.full_potential()) {
        gkvec_ = Gvec(vk_, ctx_.unit_cell().reciprocal_lattice_vectors(), gk_cutoff, ctx_.fft().grid(), num_ranks(),
                      ctx_.mpi_grid_fft().communicator(1 << 0), ctx_.gamma_point());
    } else {
        gkvec_ = Gvec(vk_, ctx_.unit_cell().reciprocal_lattice_vectors(), gk_cutoff, ctx_.fft_coarse().grid(), num_ranks(),
                      ctx_.mpi_grid_fft().communicator(1 << 0), ctx_.gamma_point());
        /* auxiliary partition for Hloc */
        gkvec_vloc_ = std::unique_ptr<Gvec_partition>(new Gvec_partition(gkvec_, ctx_.mpi_grid_fft_vloc().communicator(1 << 0)));
    }
}