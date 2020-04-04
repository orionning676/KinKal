#ifndef KinKal_TLine_hh
#define KinKal_TLine_hh
//
//  Linear time-based trajectory with a constant velocity.
//  Used as part of the kinematic Kalman fit
//
#include "KinKal/TTraj.hh"
#include "KinKal/PData.hh"
namespace KinKal {
  class TLine : public TTraj {
    public:
      enum ParamIndex {d0_=0,phi0_=1,z0_=2,cost_=3,t0_=4,npars_=5};
      constexpr static size_t NParams() { return npars_; }
      typedef PData<npars_> PDATA; // Data payload for this class
      static std::vector<std::string> const& paramNames(); 
      static std::vector<std::string> const& paramTitles();
      static std::string const& paramName(ParamIndex index);
      static std::string const& paramTitle(ParamIndex index);
 
      // construct from a spacepoint and propagation velocity (mm/ns)
      // by default, the line has infinite unforced range
      TLine(Vec4 const& p0, Vec3 const& svel, TRange const& range=TRange(),bool forcerange=false);
      TLine(Vec3 const& p0, Vec3 const& svel, double tmeas, TRange const& range=TRange(),bool forcerange=false);
      virtual ~TLine(){}  
      PDATA const& params() const { return pars_; }
    // named parameter accessors
      double param(size_t index) const { return pars_.parameters()[index]; }
      double d0() const { return param(d0_); }
      double phi0() const { return param(phi0_); }
      double z0() const { return param(z0_); }
      double cost() const { return param(cost_); }
      double t0() const { return param(t0_); }
    
      // simple functions 
      double cosTheta() const { return cost(); }
      double sinTheta() const { return sqrt(1.0-cost()*cost()); }

      // cached values
      Vec3 const& pos0() const { return pos0_; }
      Vec3 const& dir() const { return dir_; }
      double speed() const { return speed_; }
      // are we forcing the range?
      bool forceRange() const { return forcerange_; }
      // TOCA for a given point
      double TOCA(Vec3 point) const;

      // geometric accessors
      virtual void position(Vec4& pos) const override;
      virtual void position(double time, Vec3& pos) const override;
      virtual void velocity(double time, Vec3& vel) const override;
      virtual void direction(double time, Vec3& dir) const override;
      virtual double speed(double time) const override;
      virtual void print(std::ostream& ost, int detail) const override;

    private:
      PData<npars_> pars_; // parameters
      double speed_; // signed linear velocity, translates time to distance along the trajectory (mm/nsec)
      Vec3 pos0_, dir_; // caches
      bool forcerange_; // if set, strictly enforce the range

      static std::vector<std::string> paramTitles_;
      static std::vector<std::string> paramNames_;
    // nonconst accessors
      double& param(size_t index)  { return pars_.parameters()[index]; }
  };
  std::ostream& operator <<(std::ostream& ost, TLine const& tline);
}
#endif
