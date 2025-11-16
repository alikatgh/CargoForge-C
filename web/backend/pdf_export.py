"""
PDF Export Module for CargoForge
Generates professional cargo loading plan reports
"""

from reportlab.lib.pagesizes import A4, letter
from reportlab.lib import colors
from reportlab.lib.units import inch
from reportlab.platypus import SimpleDocTemplate, Table, TableStyle, Paragraph, Spacer, PageBreak, Image
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
from reportlab.pdfgen import canvas
from reportlab.graphics.shapes import Drawing, Rect, String
from reportlab.graphics import renderPDF
from datetime import datetime
import io
import os


class CargoLoadingPlanPDF:
    """Generate professional cargo loading plan PDF reports"""

    def __init__(self):
        self.styles = getSampleStyleSheet()
        self._setup_custom_styles()

    def _setup_custom_styles(self):
        """Setup custom paragraph styles"""
        # Title style
        self.styles.add(ParagraphStyle(
            name='CustomTitle',
            parent=self.styles['Heading1'],
            fontSize=24,
            textColor=colors.HexColor('#667eea'),
            spaceAfter=30,
            alignment=TA_CENTER,
            fontName='Helvetica-Bold'
        ))

        # Subtitle style
        self.styles.add(ParagraphStyle(
            name='CustomSubtitle',
            parent=self.styles['Heading2'],
            fontSize=16,
            textColor=colors.HexColor('#667eea'),
            spaceAfter=12,
            spaceBefore=12,
            fontName='Helvetica-Bold'
        ))

        # Section header
        self.styles.add(ParagraphStyle(
            name='SectionHeader',
            parent=self.styles['Heading3'],
            fontSize=12,
            textColor=colors.HexColor('#667eea'),
            spaceAfter=6,
            spaceBefore=10,
            fontName='Helvetica-Bold'
        ))

    def _create_header_footer(self, canvas_obj, doc):
        """Add header and footer to each page"""
        canvas_obj.saveState()

        # Header
        canvas_obj.setFont('Helvetica-Bold', 10)
        canvas_obj.setFillColor(colors.HexColor('#667eea'))
        canvas_obj.drawString(inch, doc.height + doc.topMargin + 0.3*inch,
                            "CargoForge - Maritime Cargo Loading Plan")

        # Footer
        canvas_obj.setFont('Helvetica', 8)
        canvas_obj.setFillColor(colors.grey)
        canvas_obj.drawString(inch, 0.5*inch,
                            f"Generated on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        canvas_obj.drawRightString(doc.width + inch, 0.5*inch,
                                  f"Page {doc.page}")

        # Watermark for free tier
        if hasattr(doc, 'tier') and doc.tier == 'free':
            canvas_obj.setFont('Helvetica', 40)
            canvas_obj.setFillColor(colors.lightgrey)
            canvas_obj.setFillAlpha(0.3)
            canvas_obj.saveState()
            canvas_obj.translate(doc.width/2, doc.height/2)
            canvas_obj.rotate(45)
            canvas_obj.drawCentredString(0, 0, "FREE TIER")
            canvas_obj.restoreState()

        canvas_obj.restoreState()

    def _create_ship_info_table(self, ship_data):
        """Create ship specifications table"""
        data = [
            ['Ship Specifications', ''],
            ['Length', f"{ship_data.get('length', 0)} m"],
            ['Width', f"{ship_data.get('width', 0)} m"],
            ['Depth', f"{ship_data.get('depth', 0)} m"],
            ['Max Draft', f"{ship_data.get('max_draft', 0)} m"],
            ['Displacement', f"{ship_data.get('displacement', 0)} tons"],
        ]

        table = Table(data, colWidths=[3*inch, 2*inch])
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#667eea')),
            ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
            ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
            ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
            ('FONTSIZE', (0, 0), (-1, 0), 12),
            ('BOTTOMPADDING', (0, 0), (-1, 0), 12),
            ('BACKGROUND', (0, 1), (-1, -1), colors.beige),
            ('GRID', (0, 0), (-1, -1), 1, colors.black),
            ('FONTNAME', (0, 1), (-1, -1), 'Helvetica'),
            ('FONTSIZE', (0, 1), (-1, -1), 10),
            ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.lightgrey]),
        ]))

        return table

    def _create_cargo_manifest_table(self, cargo_data):
        """Create detailed cargo manifest table"""
        headers = [['ID', 'Name', 'Type', 'Weight (t)', 'Dimensions (m)', 'Position']]

        rows = []
        for i, cargo in enumerate(cargo_data, 1):
            position = f"({cargo.get('x', 0):.1f}, {cargo.get('y', 0):.1f}, {cargo.get('z', 0):.1f})" if cargo.get('placed') else 'UNPLACED'
            rows.append([
                str(i),
                cargo.get('name', f'Cargo {i}'),
                cargo.get('type', 'standard'),
                f"{cargo.get('weight', 0):.1f}",
                f"{cargo.get('length', 0):.1f} × {cargo.get('width', 0):.1f} × {cargo.get('height', 0):.1f}",
                position
            ])

        data = headers + rows

        col_widths = [0.5*inch, 1.5*inch, 1*inch, 1*inch, 1.5*inch, 1.5*inch]
        table = Table(data, colWidths=col_widths)
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#667eea')),
            ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
            ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
            ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
            ('FONTSIZE', (0, 0), (-1, 0), 10),
            ('BOTTOMPADDING', (0, 0), (-1, 0), 12),
            ('BACKGROUND', (0, 1), (-1, -1), colors.beige),
            ('GRID', (0, 0), (-1, -1), 1, colors.black),
            ('FONTNAME', (0, 1), (-1, -1), 'Helvetica'),
            ('FONTSIZE', (0, 1), (-1, -1), 9),
            ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.lightgrey]),
        ]))

        return table

    def _create_stability_analysis_table(self, analysis_data):
        """Create stability analysis results table"""
        gm = analysis_data.get('gm', 0)

        # Determine stability status
        if gm < 0:
            status = 'CRITICAL - UNSTABLE'
            status_color = colors.red
        elif gm < 0.3:
            status = 'WARNING - Low Stability'
            status_color = colors.orange
        elif gm > 3.0:
            status = 'WARNING - Over-stiff'
            status_color = colors.orange
        else:
            status = 'OPTIMAL'
            status_color = colors.green

        data = [
            ['Stability Analysis', 'Value', 'Status'],
            ['Metacentric Height (GM)', f"{gm:.2f} m", status],
            ['Center of Gravity (KG)', f"{analysis_data.get('kg', 0):.2f} m", ''],
            ['Longitudinal Balance', f"{analysis_data.get('longitudinal_balance', 0):.2f} %", ''],
            ['Lateral Balance', f"{analysis_data.get('lateral_balance', 0):.2f} %", ''],
            ['Cargo Utilization', f"{analysis_data.get('cargo_utilization', 0):.1f} %", ''],
            ['Weight Utilization', f"{analysis_data.get('weight_utilization', 0):.1f} %", ''],
        ]

        table = Table(data, colWidths=[3*inch, 1.5*inch, 1.5*inch])
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#667eea')),
            ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
            ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
            ('ALIGN', (1, 0), (-1, -1), 'CENTER'),
            ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
            ('FONTSIZE', (0, 0), (-1, 0), 12),
            ('BOTTOMPADDING', (0, 0), (-1, 0), 12),
            ('BACKGROUND', (0, 1), (-1, -1), colors.beige),
            ('GRID', (0, 0), (-1, -1), 1, colors.black),
            ('FONTNAME', (0, 1), (-1, -1), 'Helvetica'),
            ('FONTSIZE', (0, 1), (-1, -1), 10),
            ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.lightgrey]),
            ('TEXTCOLOR', (2, 1), (2, 1), status_color),
            ('FONTNAME', (2, 1), (2, 1), 'Helvetica-Bold'),
        ]))

        return table

    def _create_2d_cargo_layout(self, ship_data, cargo_data):
        """Create 2D top-down view of cargo layout"""
        drawing = Drawing(400, 300)

        # Ship dimensions
        ship_length = ship_data.get('length', 150)
        ship_width = ship_data.get('width', 25)

        # Scale to fit drawing
        scale_x = 380 / ship_length
        scale_y = 280 / ship_width
        scale = min(scale_x, scale_y)

        # Draw ship outline
        ship_rect = Rect(10, 10, ship_length * scale, ship_width * scale)
        ship_rect.strokeColor = colors.HexColor('#2c3e50')
        ship_rect.fillColor = colors.HexColor('#ecf0f1')
        ship_rect.strokeWidth = 2
        drawing.add(ship_rect)

        # Color map for cargo types
        cargo_colors = {
            'standard': colors.HexColor('#667eea'),
            'hazardous': colors.HexColor('#ff6b6b'),
            'reefer': colors.HexColor('#4dabf7'),
            'fragile': colors.HexColor('#ffd43b'),
            'heavy': colors.HexColor('#9c27b0'),
            'oversized': colors.HexColor('#ff9800'),
        }

        # Draw cargo boxes
        for i, cargo in enumerate(cargo_data):
            if cargo.get('placed'):
                x = cargo.get('x', 0) * scale + 10
                y = cargo.get('y', 0) * scale + 10
                width = cargo.get('length', 0) * scale
                height = cargo.get('width', 0) * scale

                cargo_type = cargo.get('type', 'standard')
                color = cargo_colors.get(cargo_type, colors.blue)

                box = Rect(x, y, width, height)
                box.strokeColor = colors.black
                box.fillColor = color
                box.strokeWidth = 1
                drawing.add(box)

                # Add label
                label = String(x + width/2, y + height/2, str(i+1))
                label.fontSize = 8
                label.fillColor = colors.white
                label.textAnchor = 'middle'
                drawing.add(label)

        # Add legend
        legend_y = 10
        for cargo_type, color in cargo_colors.items():
            legend_rect = Rect(10, legend_y, 10, 10)
            legend_rect.fillColor = color
            legend_rect.strokeColor = colors.black
            drawing.add(legend_rect)

            legend_text = String(25, legend_y + 3, cargo_type.capitalize())
            legend_text.fontSize = 8
            drawing.add(legend_text)

            legend_y += 15

        return drawing

    def generate_loading_plan(self, optimization_data, user_tier='free', scenario_name='Untitled'):
        """
        Generate complete loading plan PDF

        Args:
            optimization_data: Dictionary containing ship, cargo, and analysis data
            user_tier: User's subscription tier ('free', 'pro', 'enterprise')
            scenario_name: Name of the scenario

        Returns:
            BytesIO buffer containing PDF data
        """
        buffer = io.BytesIO()

        # Create document
        doc = SimpleDocTemplate(
            buffer,
            pagesize=letter,
            rightMargin=inch,
            leftMargin=inch,
            topMargin=inch,
            bottomMargin=inch
        )
        doc.tier = user_tier  # For watermark

        # Container for PDF elements
        story = []

        # Title page
        story.append(Paragraph("CargoForge", self.styles['CustomTitle']))
        story.append(Paragraph("Maritime Cargo Loading Plan", self.styles['CustomSubtitle']))
        story.append(Spacer(1, 0.3*inch))

        # Scenario info
        info_text = f"""
        <b>Scenario:</b> {scenario_name}<br/>
        <b>Generated:</b> {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}<br/>
        <b>Tier:</b> {user_tier.upper()}<br/>
        """
        story.append(Paragraph(info_text, self.styles['Normal']))
        story.append(Spacer(1, 0.5*inch))

        # Ship specifications
        story.append(Paragraph("1. Ship Specifications", self.styles['SectionHeader']))
        ship_table = self._create_ship_info_table(optimization_data.get('ship', {}))
        story.append(ship_table)
        story.append(Spacer(1, 0.3*inch))

        # Cargo manifest
        story.append(Paragraph("2. Cargo Manifest", self.styles['SectionHeader']))
        cargo_data = optimization_data.get('cargo', [])
        cargo_table = self._create_cargo_manifest_table(cargo_data)
        story.append(cargo_table)
        story.append(Spacer(1, 0.3*inch))

        # Stability analysis
        story.append(Paragraph("3. Stability Analysis", self.styles['SectionHeader']))
        analysis_table = self._create_stability_analysis_table(optimization_data.get('analysis', {}))
        story.append(analysis_table)
        story.append(Spacer(1, 0.3*inch))

        # Add page break before diagrams
        story.append(PageBreak())

        # Cargo layout diagram
        story.append(Paragraph("4. Cargo Layout (Top View)", self.styles['SectionHeader']))
        layout_drawing = self._create_2d_cargo_layout(
            optimization_data.get('ship', {}),
            cargo_data
        )
        story.append(layout_drawing)
        story.append(Spacer(1, 0.3*inch))

        # Recommendations
        story.append(Paragraph("5. Recommendations", self.styles['SectionHeader']))
        recommendations = self._generate_recommendations(optimization_data.get('analysis', {}))
        for rec in recommendations:
            story.append(Paragraph(f"• {rec}", self.styles['Normal']))
        story.append(Spacer(1, 0.3*inch))

        # Disclaimer
        story.append(Spacer(1, 0.5*inch))
        disclaimer = """
        <b>DISCLAIMER:</b> This loading plan is generated for planning and educational purposes.
        Always consult with qualified maritime professionals and follow all applicable regulations
        and safety procedures. CargoForge is not liable for any decisions made based on this report.
        """
        story.append(Paragraph(disclaimer, self.styles['Normal']))

        # Build PDF
        doc.build(story, onFirstPage=self._create_header_footer,
                 onLaterPages=self._create_header_footer)

        buffer.seek(0)
        return buffer

    def _generate_recommendations(self, analysis_data):
        """Generate recommendations based on analysis"""
        recommendations = []

        gm = analysis_data.get('gm', 0)

        if gm < 0:
            recommendations.append("CRITICAL: Ship is unstable. Do not proceed with loading. Redistribute cargo immediately.")
        elif gm < 0.3:
            recommendations.append("WARNING: Metacentric height is below safe limits. Consider redistributing cargo for better stability.")
        elif gm > 3.0:
            recommendations.append("Ship is over-stiff. This may cause uncomfortable rolling motion. Consider adjusting cargo distribution.")
        else:
            recommendations.append("Stability is within optimal range. Ship is safe for operation.")

        # Balance recommendations
        long_balance = abs(analysis_data.get('longitudinal_balance', 0))
        if long_balance > 10:
            recommendations.append(f"Longitudinal imbalance is {long_balance:.1f}%. Redistribute cargo along ship's length.")

        lat_balance = abs(analysis_data.get('lateral_balance', 0))
        if lat_balance > 5:
            recommendations.append(f"Lateral imbalance is {lat_balance:.1f}%. Redistribute cargo across ship's width.")

        # Utilization recommendations
        cargo_util = analysis_data.get('cargo_utilization', 0)
        if cargo_util < 60:
            recommendations.append(f"Cargo space utilization is only {cargo_util:.1f}%. Consider better arrangement or additional cargo.")
        elif cargo_util > 95:
            recommendations.append("Excellent cargo space utilization achieved.")

        # General recommendations
        recommendations.append("Verify all hazardous materials are properly separated (minimum 3m).")
        recommendations.append("Ensure all cargo is properly secured before departure.")
        recommendations.append("Monitor weather conditions and adjust loading plan if necessary.")

        return recommendations


# Convenience function
def generate_pdf_report(optimization_data, user_tier='free', scenario_name='Untitled'):
    """
    Generate PDF report for optimization results

    Args:
        optimization_data: Dictionary with ship, cargo, and analysis data
        user_tier: User subscription tier
        scenario_name: Name of the scenario

    Returns:
        BytesIO buffer containing PDF
    """
    pdf_gen = CargoLoadingPlanPDF()
    return pdf_gen.generate_loading_plan(optimization_data, user_tier, scenario_name)
