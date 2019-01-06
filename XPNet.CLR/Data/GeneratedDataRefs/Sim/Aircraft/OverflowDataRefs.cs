using System;
using System.Collections.Generic;
using System.Text;

namespace XPNet.Data
{
    public class sim_aircraft_overflowDatarefs
    {
        private readonly IXPlaneData m_data;

        internal sim_aircraft_overflowDatarefs(IXPlaneData data)
        {
            m_data = data;
        }

        /// <summary>
        ///  amount the stab moves in trim automatically as you go to redline (zero at zero airspeed)
        /// </summary>
        public IXPDataRef<float> acf_stab_delinc_to_Vne { get { return m_data.GetFloat("sim/aircraft/overflow/acf_stab_delinc_to_vne");} }

        /// <summary>
        ///  max pressurization of the fuselage
        /// </summary>
        public IXPDataRef<float> acf_max_press_diff { get { return m_data.GetFloat("sim/aircraft/overflow/acf_max_press_diff");} }

        /// <summary>
        ///  number fuel tanks - as of 860, all planes have 9 tanks and ratios for each - ratio of 0.0 means tank is not used
        /// </summary>
        public IXPDataRef<int> acf_num_tanks { get { return m_data.GetInt("sim/aircraft/overflow/acf_num_tanks");} }

        /// <summary>
        ///  auto-trim out any flight loads... numerous planes have this.
        /// </summary>
        public IXPDataRef<bool> acf_auto_trimEQ { get { return m_data.GetBool("sim/aircraft/overflow/acf_auto_trimeq");} }

        /// <summary>
        ///  Aircraft has Fuel selector
        /// </summary>
        public IXPDataRef<bool> acf_has_fuel_any { get { return m_data.GetBool("sim/aircraft/overflow/acf_has_fuel_any");} }
    }
}