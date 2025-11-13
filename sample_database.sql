-- =============================================
-- SINGLE DATABASE SETUP
-- =============================================
-- All tables are created in a single PostgreSQL database using schemas

\c postgres

DROP SCHEMA IF EXISTS users CASCADE;
DROP SCHEMA IF EXISTS products CASCADE;
DROP SCHEMA IF EXISTS cart CASCADE;
DROP SCHEMA IF EXISTS coupon CASCADE;
DROP SCHEMA IF EXISTS wallet CASCADE;
DROP SCHEMA IF EXISTS orders CASCADE;
DROP SCHEMA IF EXISTS payments CASCADE;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- =============================================
-- USERS SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS users;

CREATE TABLE users.users (
    user_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    first_name VARCHAR(100) NOT NULL,
    last_name VARCHAR(100) NOT NULL,
    phone VARCHAR(20),
    date_of_birth DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    email_verified BOOLEAN DEFAULT FALSE
);

CREATE TABLE users.user_addresses (
    address_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    address_type VARCHAR(20) CHECK (address_type IN ('shipping', 'billing', 'both')),
    address_line1 VARCHAR(255) NOT NULL,
    address_line2 VARCHAR(255),
    city VARCHAR(100) NOT NULL,
    state VARCHAR(100) NOT NULL,
    postal_code VARCHAR(20) NOT NULL,
    country VARCHAR(100) NOT NULL,
    is_default BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users.users(user_id) ON DELETE CASCADE
);

-- Insert Users (created between May-November 2025, added more for realism)
INSERT INTO users.users (email, password_hash, first_name, last_name, phone, date_of_birth, email_verified, created_at) VALUES
('john.doe@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'John', 'Doe', '+1234567890', '1990-05-15', TRUE, '2025-05-12 10:30:00'::timestamp),
('jane.smith@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Jane', 'Smith', '+1234567891', '1985-08-22', TRUE, '2025-06-20 14:15:00'::timestamp),
('mike.johnson@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Mike', 'Johnson', '+1234567892', '1992-03-10', TRUE, '2025-07-08 09:45:00'::timestamp),
('sarah.williams@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Sarah', 'Williams', '+1234567893', '1988-11-30', FALSE, '2025-09-15 16:20:00'::timestamp),
('david.brown@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'David', 'Brown', '+1234567894', '1995-07-18', TRUE, '2025-10-22 11:00:00'::timestamp),
('alice.green@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Alice', 'Green', '+1234567895', '1993-02-05', TRUE, '2025-08-05 12:00:00'::timestamp),
('bob.white@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Bob', 'White', '+1234567896', '1987-12-12', FALSE, '2025-10-01 13:30:00'::timestamp),
('emma.black@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Emma', 'Black', '+1234567897', '1991-06-25', TRUE, '2025-11-01 09:00:00'::timestamp),
('frank.red@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Frank', 'Red', '+1234567898', '1989-09-14', TRUE, '2025-11-05 15:45:00'::timestamp),
('grace.yellow@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Grace', 'Yellow', '+1234567899', '1994-04-03', FALSE, '2025-11-10 11:20:00'::timestamp);

-- Insert User Addresses (added more, including multiples for some users)
INSERT INTO users.user_addresses (user_id, address_type, address_line1, city, state, postal_code, country, is_default, created_at)
SELECT 
    user_id, 'both', '123 Main Street', 'New York', 'NY', '10001', 'USA', TRUE, '2025-05-12 10:35:00'::timestamp
FROM users.users WHERE email = 'john.doe@email.com'
UNION ALL
SELECT 
    user_id, 'shipping', '456 Oak Avenue', 'Brooklyn', 'NY', '11201', 'USA', FALSE, '2025-08-10 15:20:00'::timestamp
FROM users.users WHERE email = 'john.doe@email.com'
UNION ALL
SELECT 
    user_id, 'both', '789 Pine Road', 'Los Angeles', 'CA', '90001', 'USA', TRUE, '2025-06-20 14:20:00'::timestamp
FROM users.users WHERE email = 'jane.smith@email.com'
UNION ALL
SELECT 
    user_id, 'both', '321 Elm Street', 'Chicago', 'IL', '60601', 'USA', TRUE, '2025-07-08 09:50:00'::timestamp
FROM users.users WHERE email = 'mike.johnson@email.com'
UNION ALL
SELECT 
    user_id, 'both', '654 Maple Drive', 'Houston', 'TX', '77001', 'USA', TRUE, '2025-09-15 16:25:00'::timestamp
FROM users.users WHERE email = 'sarah.williams@email.com'
UNION ALL
SELECT 
    user_id, 'both', '987 Cedar Lane', 'Phoenix', 'AZ', '85001', 'USA', TRUE, '2025-10-22 11:05:00'::timestamp
FROM users.users WHERE email = 'david.brown@email.com'
UNION ALL
SELECT 
    user_id, 'shipping', '135 Willow Way', 'Seattle', 'WA', '98101', 'USA', TRUE, '2025-08-05 12:05:00'::timestamp
FROM users.users WHERE email = 'alice.green@email.com'
UNION ALL
SELECT 
    user_id, 'billing', '246 Birch Blvd', 'Miami', 'FL', '33101', 'USA', FALSE, '2025-10-01 13:35:00'::timestamp
FROM users.users WHERE email = 'bob.white@email.com'
UNION ALL
SELECT 
    user_id, 'both', '357 Aspen St', 'Denver', 'CO', '80201', 'USA', TRUE, '2025-11-01 09:05:00'::timestamp
FROM users.users WHERE email = 'emma.black@email.com'
UNION ALL
SELECT 
    user_id, 'both', '468 Spruce Rd', 'Boston', 'MA', '02101', 'USA', TRUE, '2025-11-05 15:50:00'::timestamp
FROM users.users WHERE email = 'frank.red@email.com'
UNION ALL
SELECT 
    user_id, 'shipping', '579 Fir Ln', 'Atlanta', 'GA', '30301', 'USA', TRUE, '2025-11-10 11:25:00'::timestamp
FROM users.users WHERE email = 'grace.yellow@email.com'
UNION ALL
SELECT 
    user_id, 'both', '690 Poplar Dr', 'Dallas', 'TX', '75201', 'USA', FALSE, '2025-11-12 10:00:00'::timestamp
FROM users.users WHERE email = 'grace.yellow@email.com';

CREATE INDEX idx_users_email ON users.users(email);
CREATE INDEX idx_user_addresses_user ON users.user_addresses(user_id);

-- =============================================
-- PRODUCTS SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS products;

CREATE TABLE products.categories (
    category_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    category_name VARCHAR(100) NOT NULL,
    parent_category_id UUID,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (parent_category_id) REFERENCES products.categories(category_id) ON DELETE SET NULL
);

CREATE TABLE products.products (
    product_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    category_id UUID,
    product_name VARCHAR(255) NOT NULL,
    description TEXT,
    sku VARCHAR(100) UNIQUE NOT NULL,
    price DECIMAL(10, 2) NOT NULL,
    cost_price DECIMAL(10, 2),
    stock_quantity INTEGER NOT NULL DEFAULT 0,
    is_active BOOLEAN DEFAULT TRUE,
    weight_kg DECIMAL(6, 2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (category_id) REFERENCES products.categories(category_id) ON DELETE SET NULL
);

CREATE TABLE products.product_images (
    image_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    product_id UUID NOT NULL,
    image_url VARCHAR(500) NOT NULL,
    is_primary BOOLEAN DEFAULT FALSE,
    display_order INTEGER DEFAULT 0,
    FOREIGN KEY (product_id) REFERENCES products.products(product_id) ON DELETE CASCADE
);

CREATE TABLE products.product_reviews (
    review_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    product_id UUID NOT NULL,
    user_id UUID NOT NULL,
    rating INTEGER CHECK (rating >= 1 AND rating <= 5),
    review_title VARCHAR(255),
    review_text TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (product_id) REFERENCES products.products(product_id) ON DELETE CASCADE
);

-- Insert Categories (created in early 2025, added more subcategories for realism)
INSERT INTO products.categories (category_name, description, created_at) VALUES
('Electronics', 'Electronic devices and gadgets', '2025-01-10 08:00:00'::timestamp),
('Clothing', 'Apparel and fashion items', '2025-01-10 08:05:00'::timestamp),
('Books', 'Books and publications', '2025-01-10 08:10:00'::timestamp),
('Home & Kitchen', 'Home appliances and kitchen items', '2025-01-10 08:15:00'::timestamp),
('Sports', 'Sports equipment and accessories', '2025-01-10 08:20:00'::timestamp),
('Audio', 'Audio devices', '2025-02-15 09:00:00'::timestamp),
('Wearables', 'Wearable technology', '2025-02-20 10:00:00'::timestamp),
('Fiction', 'Fiction books', '2025-03-01 11:00:00'::timestamp);

UPDATE products.categories SET parent_category_id = (SELECT category_id FROM products.categories WHERE category_name = 'Electronics') WHERE category_name = 'Audio';
UPDATE products.categories SET parent_category_id = (SELECT category_id FROM products.categories WHERE category_name = 'Electronics') WHERE category_name = 'Wearables';
UPDATE products.categories SET parent_category_id = (SELECT category_id FROM products.categories WHERE category_name = 'Books') WHERE category_name = 'Fiction';

-- Insert Products (added throughout 2025, added more products for variety)
INSERT INTO products.products (category_id, product_name, description, sku, price, cost_price, stock_quantity, created_at, updated_at) 
SELECT 
    category_id,
    'Wireless Headphones Pro',
    'Premium wireless headphones with noise cancellation',
    'WHP-001',
    199.99,
    120.00,
    50,
    '2025-03-15 10:00:00'::timestamp,
    '2025-11-01 14:30:00'::timestamp
FROM products.categories WHERE category_name = 'Electronics'
UNION ALL
SELECT 
    category_id,
    'Smart Watch Ultra',
    'Advanced smartwatch with health tracking',
    'SWU-002',
    299.99,
    180.00,
    35,
    '2025-04-20 11:30:00'::timestamp,
    '2025-10-28 09:15:00'::timestamp
FROM products.categories WHERE category_name = 'Electronics'
UNION ALL
SELECT 
    category_id,
    'Cotton T-Shirt',
    'Comfortable 100% cotton t-shirt',
    'CTS-003',
    29.99,
    12.00,
    200,
    '2025-02-10 09:00:00'::timestamp,
    '2025-11-05 16:45:00'::timestamp
FROM products.categories WHERE category_name = 'Clothing'
UNION ALL
SELECT 
    category_id,
    'The Complete Guide to PostgreSQL',
    'Comprehensive PostgreSQL database guide',
    'BPG-004',
    49.99,
    25.00,
    100,
    '2025-05-05 13:20:00'::timestamp,
    '2025-11-08 10:00:00'::timestamp
FROM products.categories WHERE category_name = 'Books'
UNION ALL
SELECT 
    category_id,
    'Stainless Steel Blender',
    'High-power kitchen blender',
    'SSB-005',
    89.99,
    45.00,
    75,
    '2025-06-12 15:45:00'::timestamp,
    '2025-11-09 11:30:00'::timestamp
FROM products.categories WHERE category_name = 'Home & Kitchen'
UNION ALL
SELECT 
    category_id,
    'Yoga Mat Premium',
    'Non-slip premium yoga mat',
    'YMP-006',
    39.99,
    18.00,
    150,
    '2025-07-18 08:30:00'::timestamp,
    '2025-11-10 14:20:00'::timestamp
FROM products.categories WHERE category_name = 'Sports'
UNION ALL
SELECT 
    category_id,
    'Gaming Laptop',
    'High-performance gaming laptop',
    'GLP-007',
    1299.99,
    800.00,
    20,
    '2025-08-25 12:00:00'::timestamp,
    '2025-11-12 09:30:00'::timestamp
FROM products.categories WHERE category_name = 'Electronics'
UNION ALL
SELECT 
    category_id,
    'Denim Jeans',
    'Classic denim jeans',
    'DNJ-008',
    59.99,
    25.00,
    120,
    '2025-09-10 14:15:00'::timestamp,
    '2025-11-11 10:45:00'::timestamp
FROM products.categories WHERE category_name = 'Clothing'
UNION ALL
SELECT 
    category_id,
    'Mystery Novel',
    'Thrilling mystery book',
    'MNB-009',
    19.99,
    10.00,
    300,
    '2025-10-05 16:30:00'::timestamp,
    '2025-11-10 11:00:00'::timestamp
FROM products.categories WHERE category_name = 'Fiction'
UNION ALL
SELECT 
    category_id,
    'Coffee Maker',
    'Automatic coffee maker',
    'CFM-010',
    79.99,
    40.00,
    80,
    '2025-11-01 08:45:00'::timestamp,
    '2025-11-12 13:20:00'::timestamp
FROM products.categories WHERE category_name = 'Home & Kitchen';

-- Insert Product Images (one primary per product, added for new products)
INSERT INTO products.product_images (product_id, image_url, is_primary, display_order)
SELECT 
    product_id,
    'https://cdn.example.com/images/' || sku || '-1.jpg',
    TRUE,
    1
FROM products.products;

-- Insert Product Reviews (reviews from October-November 2025, added more from different users)
INSERT INTO products.product_reviews (product_id, user_id, rating, review_title, review_text, created_at)
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'WHP-001'),
    (SELECT user_id FROM users.users WHERE email = 'john.doe@email.com'),
    5,
    'Amazing Sound Quality!',
    'These headphones are incredible. The noise cancellation works perfectly and battery life is excellent.',
    '2025-10-25 15:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'SWU-002'),
    (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com'),
    4,
    'Great smartwatch',
    'Love the features, but wish battery lasted longer.',
    '2025-11-02 10:15:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'CTS-003'),
    (SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com'),
    5,
    'Perfect fit',
    'Very comfortable and great quality cotton.',
    '2025-11-08 14:45:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'BPG-004'),
    (SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com'),
    4,
    'Useful guide',
    'Good content, but some sections are too advanced for beginners.',
    '2025-11-10 09:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'SSB-005'),
    (SELECT user_id FROM users.users WHERE email = 'david.brown@email.com'),
    5,
    'Powerful blender',
    'Blends everything smoothly, easy to clean.',
    '2025-11-11 11:00:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'YMP-006'),
    (SELECT user_id FROM users.users WHERE email = 'alice.green@email.com'),
    3,
    'Decent mat',
    'Good grip, but thinner than expected.',
    '2025-11-12 13:45:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'GLP-007'),
    (SELECT user_id FROM users.users WHERE email = 'bob.white@email.com'),
    5,
    'Excellent for gaming',
    'High FPS, great graphics card.',
    '2025-11-05 15:20:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'DNJ-008'),
    (SELECT user_id FROM users.users WHERE email = 'emma.black@email.com'),
    4,
    'Comfortable jeans',
    'Fit well, durable material.',
    '2025-11-07 10:10:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'MNB-009'),
    (SELECT user_id FROM users.users WHERE email = 'frank.red@email.com'),
    5,
    'Gripping story',
    'Could not put it down!',
    '2025-11-09 12:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT product_id FROM products.products WHERE sku = 'CFM-010'),
    (SELECT user_id FROM users.users WHERE email = 'grace.yellow@email.com'),
    4,
    'Good coffee',
    'Brews quickly, consistent taste.',
    '2025-11-11 14:50:00'::timestamp;

CREATE INDEX idx_products_category ON products.products(category_id);
CREATE INDEX idx_products_sku ON products.products(sku);
CREATE INDEX idx_product_images_product ON products.product_images(product_id);
CREATE INDEX idx_product_reviews_product ON products.product_reviews(product_id);

-- =============================================
-- CART SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS cart;

CREATE TABLE cart.cart (
    cart_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE cart.cart_items (
    cart_item_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    cart_id UUID NOT NULL,
    product_id UUID NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 1 CHECK (quantity > 0),
    added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (cart_id) REFERENCES cart.cart(cart_id) ON DELETE CASCADE
);

-- Insert Carts (active carts from November 2025, added more for new users)
INSERT INTO cart.cart (user_id, created_at, updated_at) VALUES
((SELECT user_id FROM users.users WHERE email = 'john.doe@email.com'), '2025-11-09 10:30:00'::timestamp, '2025-11-09 10:35:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com'), '2025-11-10 14:20:00'::timestamp, '2025-11-10 15:10:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com'), '2025-11-11 09:00:00'::timestamp, '2025-11-11 09:15:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'alice.green@email.com'), '2025-11-12 12:00:00'::timestamp, '2025-11-12 12:30:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'bob.white@email.com'), '2025-11-08 13:45:00'::timestamp, '2025-11-08 14:00:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'emma.black@email.com'), '2025-11-07 10:20:00'::timestamp, '2025-11-07 10:40:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'frank.red@email.com'), '2025-11-06 15:15:00'::timestamp, '2025-11-06 15:25:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'grace.yellow@email.com'), '2025-11-05 11:30:00'::timestamp, '2025-11-05 11:45:00'::timestamp);

-- Insert Cart Items (added in November 2025, added more items and for new carts)
INSERT INTO cart.cart_items (cart_id, product_id, quantity, added_at)
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'john.doe@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'WHP-001'),
    1,
    '2025-11-09 10:35:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'SWU-002'),
    1,
    '2025-11-10 14:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'CTS-003'),
    2,
    '2025-11-10 15:10:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'BPG-004'),
    1,
    '2025-11-11 09:05:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'alice.green@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'YMP-006'),
    3,
    '2025-11-12 12:15:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'bob.white@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'GLP-007'),
    1,
    '2025-11-08 13:50:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'emma.black@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'DNJ-008'),
    2,
    '2025-11-07 10:25:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'frank.red@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'MNB-009'),
    1,
    '2025-11-06 15:20:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'grace.yellow@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'CFM-010'),
    1,
    '2025-11-05 11:35:00'::timestamp
UNION ALL
SELECT 
    (SELECT cart_id FROM cart.cart WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'grace.yellow@email.com')),
    (SELECT product_id FROM products.products WHERE sku = 'SSB-005'),
    1,
    '2025-11-05 11:40:00'::timestamp;

CREATE INDEX idx_cart_user ON cart.cart(user_id);
CREATE INDEX idx_cart_items_cart ON cart.cart_items(cart_id);

-- =============================================
-- COUPON SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS coupon;

CREATE TABLE coupon.coupons (
    coupon_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    coupon_code VARCHAR(50) UNIQUE NOT NULL,
    description TEXT,
    discount_type VARCHAR(20) CHECK (discount_type IN ('percentage', 'fixed')),
    discount_value DECIMAL(10, 2) NOT NULL,
    min_purchase_amount DECIMAL(10, 2),
    max_discount_amount DECIMAL(10, 2),
    usage_limit INTEGER,
    usage_count INTEGER DEFAULT 0,
    start_date TIMESTAMP NOT NULL,
    end_date TIMESTAMP NOT NULL,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE coupon.user_coupon_usage (
    usage_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    coupon_id UUID NOT NULL,
    used_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (coupon_id) REFERENCES coupon.coupons(coupon_id) ON DELETE CASCADE
);

-- Insert Coupons (active in November 2025, added more for variety)
INSERT INTO coupon.coupons (coupon_code, description, discount_type, discount_value, min_purchase_amount, max_discount_amount, usage_limit, start_date, end_date, is_active, usage_count, created_at) VALUES
('WELCOME10', '10% off on first purchase', 'percentage', 10.00, 50.00, 50.00, 1000, '2025-01-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 245, '2025-01-01 00:00:00'::timestamp),
('SAVE50', 'Flat $50 off on orders above $200', 'fixed', 50.00, 200.00, 50.00, 500, '2025-06-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 87, '2025-06-01 00:00:00'::timestamp),
('BLACKFRIDAY25', '25% off Black Friday sale', 'percentage', 25.00, 100.00, 100.00, 5000, '2025-11-01 00:00:00'::timestamp, '2025-11-30 23:59:59'::timestamp, TRUE, 1523, '2025-10-25 00:00:00'::timestamp),
('FREESHIP', 'Free shipping on all orders', 'fixed', 10.00, 0.00, 10.00, 10000, '2025-01-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 3421, '2025-01-01 00:00:00'::timestamp),
('NEWYEAR20', '20% off for New Year', 'percentage', 20.00, 75.00, 75.00, 2000, '2025-12-15 00:00:00'::timestamp, '2026-01-15 23:59:59'::timestamp, TRUE, 0, '2025-11-10 00:00:00'::timestamp),
('LOYALTY5', '5% off for loyal customers', 'percentage', 5.00, 0.00, 20.00, 500, '2025-07-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 120, '2025-07-01 00:00:00'::timestamp);

-- Insert Coupon Usage (recent usage in October-November 2025, added more)
INSERT INTO coupon.user_coupon_usage (user_id, coupon_id, used_at)
SELECT 
    (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com'),
    (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'WELCOME10'),
    '2025-10-18 15:45:00'::timestamp
UNION ALL
SELECT 
    (SELECT user_id FROM users.users WHERE email = 'david.brown@email.com'),
    (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'FREESHIP'),
    '2025-11-05 11:20:00'::timestamp
UNION ALL
SELECT 
    (SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com'),
    (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'SAVE50'),
    '2025-11-08 14:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT user_id FROM users.users WHERE email = 'alice.green@email.com'),
    (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'BLACKFRIDAY25'),
    '2025-11-10 09:45:00'::timestamp
UNION ALL
SELECT 
    (SELECT user_id FROM users.users WHERE email = 'bob.white@email.com'),
    (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'LOYALTY5'),
    '2025-11-12 13:00:00'::timestamp
UNION ALL
SELECT 
    (SELECT user_id FROM users.users WHERE email = 'emma.black@email.com'),
    (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'FREESHIP'),
    '2025-11-07 10:15:00'::timestamp;

CREATE INDEX idx_coupons_code ON coupon.coupons(coupon_code);
CREATE INDEX idx_user_coupon_usage_user ON coupon.user_coupon_usage(user_id);

-- =============================================
-- WALLET SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS wallet;

CREATE TABLE wallet.wallets (
    wallet_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID UNIQUE NOT NULL,
    balance DECIMAL(10, 2) DEFAULT 0.00 CHECK (balance >= 0),
    currency VARCHAR(3) DEFAULT 'USD',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE wallet.wallet_transactions (
    transaction_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    wallet_id UUID NOT NULL,
    transaction_type VARCHAR(20) CHECK (transaction_type IN ('credit', 'debit', 'refund')),
    amount DECIMAL(10, 2) NOT NULL,
    balance_after DECIMAL(10, 2) NOT NULL,
    description TEXT,
    reference_id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (wallet_id) REFERENCES wallet.wallets(wallet_id) ON DELETE CASCADE
);

-- Insert Wallets (created with user registration, added for all users)
INSERT INTO wallet.wallets (user_id, balance, created_at, updated_at) VALUES
((SELECT user_id FROM users.users WHERE email = 'john.doe@email.com'), 150.00, '2025-05-12 10:30:00'::timestamp, '2025-11-03 14:20:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com'), 250.50, '2025-06-20 14:15:00'::timestamp, '2025-10-25 16:30:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com'), 75.25, '2025-07-08 09:45:00'::timestamp, '2025-11-01 10:15:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com'), 0.00, '2025-09-15 16:20:00'::timestamp, '2025-09-15 16:20:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'david.brown@email.com'), 500.00, '2025-10-22 11:00:00'::timestamp, '2025-11-06 09:45:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'alice.green@email.com'), 120.75, '2025-08-05 12:00:00'::timestamp, '2025-11-10 13:30:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'bob.white@email.com'), 300.00, '2025-10-01 13:30:00'::timestamp, '2025-11-08 15:00:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'emma.black@email.com'), 45.50, '2025-11-01 09:00:00'::timestamp, '2025-11-07 11:20:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'frank.red@email.com'), 200.00, '2025-11-05 15:45:00'::timestamp, '2025-11-09 12:45:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'grace.yellow@email.com'), 80.00, '2025-11-10 11:20:00'::timestamp, '2025-11-12 14:10:00'::timestamp);

-- Insert Wallet Transactions (transactions from October-November 2025, added more variety including refunds)
INSERT INTO wallet.wallet_transactions (wallet_id, transaction_type, amount, balance_after, description, created_at)
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'john.doe@email.com')),
    'credit',
    100.00,
    100.00,
    'Welcome bonus credited',
    '2025-05-12 10:35:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'john.doe@email.com')),
    'credit',
    50.00,
    150.00,
    'Referral bonus',
    '2025-11-03 14:20:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com')),
    'credit',
    300.00,
    300.00,
    'Account top-up',
    '2025-10-15 12:00:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com')),
    'debit',
    49.50,
    250.50,
    'Payment for order ORD-2025-001',
    '2025-10-25 16:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com')),
    'credit',
    100.00,
    100.00,
    'Welcome bonus',
    '2025-07-08 09:50:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com')),
    'debit',
    24.75,
    75.25,
    'Payment for subscription',
    '2025-11-01 10:15:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'david.brown@email.com')),
    'credit',
    500.00,
    500.00,
    'Account top-up',
    '2025-10-22 11:05:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'alice.green@email.com')),
    'refund',
    20.75,
    120.75,
    'Refund for cancelled order',
    '2025-11-10 13:30:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'bob.white@email.com')),
    'credit',
    300.00,
    300.00,
    'Gift card added',
    '2025-10-01 13:35:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'emma.black@email.com')),
    'debit',
    54.50,
    45.50,
    'Payment for order',
    '2025-11-07 11:20:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'frank.red@email.com')),
    'credit',
    200.00,
    200.00,
    'Account top-up',
    '2025-11-05 15:50:00'::timestamp
UNION ALL
SELECT 
    (SELECT wallet_id FROM wallet.wallets WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'grace.yellow@email.com')),
    'credit',
    80.00,
    80.00,
    'Referral bonus',
    '2025-11-10 11:25:00'::timestamp;

CREATE INDEX idx_wallets_user ON wallet.wallets(user_id);
CREATE INDEX idx_wallet_transactions_wallet ON wallet.wallet_transactions(wallet_id);

-- =============================================
-- ORDERS SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS orders;

CREATE TABLE orders.orders (
    order_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    order_number VARCHAR(50) UNIQUE NOT NULL,
    order_status VARCHAR(20) CHECK (order_status IN ('pending', 'confirmed', 'processing', 'shipped', 'delivered', 'cancelled', 'refunded')),
    subtotal DECIMAL(10, 2) NOT NULL,
    discount_amount DECIMAL(10, 2) DEFAULT 0.00,
    tax_amount DECIMAL(10, 2) DEFAULT 0.00,
    shipping_amount DECIMAL(10, 2) DEFAULT 0.00,
    total_amount DECIMAL(10, 2) NOT NULL,
    coupon_id UUID,
    shipping_address_id UUID,
    billing_address_id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE orders.order_items (
    order_item_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    order_id UUID NOT NULL,
    product_id UUID NOT NULL,
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    unit_price DECIMAL(10, 2) NOT NULL,
    total_price DECIMAL(10, 2) NOT NULL,
    FOREIGN KEY (order_id) REFERENCES orders.orders(order_id) ON DELETE CASCADE
);

CREATE TABLE orders.order_status_history (
    history_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    order_id UUID NOT NULL,
    status VARCHAR(20) NOT NULL,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (order_id) REFERENCES orders.orders(order_id) ON DELETE CASCADE
);

-- Insert Orders (orders from October-November 2025, added more with varied statuses)
INSERT INTO orders.orders (user_id, order_number, order_status, subtotal, discount_amount, tax_amount, shipping_amount, total_amount, coupon_id, shipping_address_id, billing_address_id, created_at, updated_at) VALUES
((SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com'), 'ORD-2025-001', 'delivered', 299.99, 30.00, 24.00, 10.00, 303.99, (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'WELCOME10'), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'jane.smith@email.com') AND is_default = TRUE), '2025-10-25 15:30:00'::timestamp, '2025-11-02 10:20:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com'), 'ORD-2025-002', 'shipped', 89.99, 0.00, 7.20, 10.00, 107.19, NULL, (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'mike.johnson@email.com') AND is_default = TRUE), '2025-11-06 11:15:00'::timestamp, '2025-11-09 14:30:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'david.brown@email.com'), 'ORD-2025-003', 'processing', 199.99, 0.00, 16.00, 0.00, 215.99, (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'FREESHIP'), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'david.brown@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'david.brown@email.com') AND is_default = TRUE), '2025-11-10 09:45:00'::timestamp, '2025-11-10 16:30:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'john.doe@email.com'), 'ORD-2025-004', 'confirmed', 129.96, 32.49, 10.40, 10.00, 117.87, (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'BLACKFRIDAY25'), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'john.doe@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'john.doe@email.com') AND is_default = TRUE), '2025-11-11 08:20:00'::timestamp, '2025-11-11 08:35:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com'), 'ORD-2025-005', 'cancelled', 49.99, 0.00, 4.00, 5.00, 58.99, NULL, (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'sarah.williams@email.com') AND is_default = TRUE), '2025-11-12 10:00:00'::timestamp, '2025-11-12 11:00:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'alice.green@email.com'), 'ORD-2025-006', 'delivered', 119.97, 6.00, 9.60, 0.00, 123.57, (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'LOYALTY5'), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'alice.green@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'alice.green@email.com') AND is_default = TRUE), '2025-10-30 13:45:00'::timestamp, '2025-11-05 15:30:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'bob.white@email.com'), 'ORD-2025-007', 'refunded', 1299.99, 260.00, 104.00, 20.00, 1163.99, (SELECT coupon_id FROM coupon.coupons WHERE coupon_code = 'NEWYEAR20'), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'bob.white@email.com') AND address_type = 'billing'), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'bob.white@email.com') AND address_type = 'billing'), '2025-11-08 14:00:00'::timestamp, '2025-11-10 16:45:00'::timestamp),
((SELECT user_id FROM users.users WHERE email = 'emma.black@email.com'), 'ORD-2025-008', 'shipped', 59.99, 0.00, 4.80, 10.00, 74.79, NULL, (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'emma.black@email.com') AND is_default = TRUE), (SELECT address_id FROM users.user_addresses WHERE user_id = (SELECT user_id FROM users.users WHERE email = 'emma.black@email.com') AND is_default = TRUE), '2025-11-07 10:40:00'::timestamp, '2025-11-09 12:00:00'::timestamp);

-- Insert Order Items (added for new orders)
INSERT INTO orders.order_items (order_id, product_id, quantity, unit_price, total_price)
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'),
    (SELECT product_id FROM products.products WHERE sku = 'SWU-002'),
    1,
    299.99,
    299.99
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-002'),
    (SELECT product_id FROM products.products WHERE sku = 'SSB-005'),
    1,
    89.99,
    89.99
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-003'),
    (SELECT product_id FROM products.products WHERE sku = 'WHP-001'),
    1,
    199.99,
    199.99
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-004'),
    (SELECT product_id FROM products.products WHERE sku = 'CTS-003'),
    3,
    29.99,
    89.97
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-004'),
    (SELECT product_id FROM products.products WHERE sku = 'YMP-006'),
    1,
    39.99,
    39.99
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-005'),
    (SELECT product_id FROM products.products WHERE sku = 'BPG-004'),
    1,
    49.99,
    49.99
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'),
    (SELECT product_id FROM products.products WHERE sku = 'YMP-006'),
    3,
    39.99,
    119.97
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'),
    (SELECT product_id FROM products.products WHERE sku = 'GLP-007'),
    1,
    1299.99,
    1299.99
UNION ALL
SELECT 
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-008'),
    (SELECT product_id FROM products.products WHERE sku = 'DNJ-008'),
    1,
    59.99,
    59.99;

-- Insert Order Status History (added for new orders)
INSERT INTO orders.order_status_history (order_id, status, notes, created_at)
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'), 'pending', 'Order placed', '2025-10-25 15:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'), 'confirmed', 'Payment confirmed', '2025-10-25 16:15:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'), 'processing', 'Order being prepared', '2025-10-26 09:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'), 'shipped', 'Order shipped via FedEx', '2025-10-28 10:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'), 'delivered', 'Order delivered successfully', '2025-11-02 10:20:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-002'), 'pending', 'Order placed', '2025-11-06 11:15:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-002'), 'confirmed', 'Payment confirmed', '2025-11-06 11:45:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-002'), 'processing', 'Order being prepared', '2025-11-07 08:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-002'), 'shipped', 'Order shipped via UPS', '2025-11-09 14:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-003'), 'pending', 'Order placed', '2025-11-10 09:45:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-003'), 'confirmed', 'Payment confirmed via wallet', '2025-11-10 10:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-003'), 'processing', 'Order being prepared', '2025-11-10 16:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-004'), 'pending', 'Order placed', '2025-11-11 08:20:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-004'), 'confirmed', 'Payment confirmed', '2025-11-11 08:35:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-005'), 'pending', 'Order placed', '2025-11-12 10:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-005'), 'cancelled', 'Cancelled by user', '2025-11-12 11:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'), 'pending', 'Order placed', '2025-10-30 13:45:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'), 'confirmed', 'Payment confirmed', '2025-10-30 14:15:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'), 'processing', 'Order being prepared', '2025-10-31 09:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'), 'shipped', 'Order shipped via DHL', '2025-11-02 11:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'), 'delivered', 'Delivered', '2025-11-05 15:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'), 'pending', 'Order placed', '2025-11-08 14:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'), 'confirmed', 'Payment confirmed', '2025-11-08 14:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'), 'processing', 'Order being prepared', '2025-11-09 10:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'), 'refunded', 'Refunded due to stock issue', '2025-11-10 16:45:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-008'), 'pending', 'Order placed', '2025-11-07 10:40:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-008'), 'confirmed', 'Payment confirmed', '2025-11-07 11:00:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-008'), 'processing', 'Order being prepared', '2025-11-08 09:30:00'::timestamp
UNION ALL
SELECT (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-008'), 'shipped', 'Shipped via FedEx', '2025-11-09 12:00:00'::timestamp;

CREATE INDEX idx_orders_user ON orders.orders(user_id);
CREATE INDEX idx_orders_status ON orders.orders(order_status);
CREATE INDEX idx_orders_number ON orders.orders(order_number);
CREATE INDEX idx_order_items_order ON orders.order_items(order_id);
CREATE INDEX idx_order_status_history_order ON orders.order_status_history(order_id);

-- =============================================
-- PAYMENTS SCHEMA
-- =============================================

CREATE SCHEMA IF NOT EXISTS payments;

CREATE TABLE payments.payments (
    payment_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    order_id UUID NOT NULL,
    payment_method VARCHAR(50) CHECK (payment_method IN ('credit_card', 'debit_card', 'wallet', 'upi', 'net_banking', 'cod')),
    payment_status VARCHAR(20) CHECK (payment_status IN ('pending', 'completed', 'failed', 'refunded')),
    amount DECIMAL(10, 2) NOT NULL,
    transaction_id VARCHAR(255),
    payment_gateway VARCHAR(50),
    payment_date TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE payments.refunds (
    refund_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    payment_id UUID NOT NULL,
    order_id UUID NOT NULL,
    refund_amount DECIMAL(10, 2) NOT NULL,
    refund_reason TEXT,
    refund_status VARCHAR(20) CHECK (refund_status IN ('pending', 'processed', 'completed', 'rejected')),
    refund_method VARCHAR(50) CHECK (refund_method IN ('original_payment', 'wallet', 'bank_transfer')),
    processed_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (payment_id) REFERENCES payments.payments(payment_id) ON DELETE CASCADE
);

-- Insert Payments (payments from October-November 2025, added for new orders)
INSERT INTO payments.payments (order_id, payment_method, payment_status, amount, transaction_id, payment_gateway, payment_date, created_at) VALUES
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'), 'credit_card', 'completed', 303.99, 'TXN-STRIPE-20251025153045', 'Stripe', '2025-10-25 16:15:00'::timestamp, '2025-10-25 15:30:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-002'), 'upi', 'completed', 107.19, 'TXN-RAZORPAY-20251106111530', 'Razorpay', '2025-11-06 11:45:00'::timestamp, '2025-11-06 11:15:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-003'), 'wallet', 'completed', 215.99, 'TXN-WALLET-20251110094520', 'Internal', '2025-11-10 10:00:00'::timestamp, '2025-11-10 09:45:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-004'), 'debit_card', 'completed', 117.87, 'TXN-STRIPE-20251111082035', 'Stripe', '2025-11-11 08:35:00'::timestamp, '2025-11-11 08:20:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-005'), 'cod', 'failed', 58.99, 'TXN-COD-20251112100000', 'Internal', NULL, '2025-11-12 10:00:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-006'), 'net_banking', 'completed', 123.57, 'TXN-BANK-20251030134500', 'BankGateway', '2025-10-30 14:15:00'::timestamp, '2025-10-30 13:45:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'), 'credit_card', 'refunded', 1163.99, 'TXN-STRIPE-20251108140000', 'Stripe', '2025-11-08 14:30:00'::timestamp, '2025-11-08 14:00:00'::timestamp),
((SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-008'), 'upi', 'completed', 74.79, 'TXN-RAZORPAY-20251107104000', 'Razorpay', '2025-11-07 11:00:00'::timestamp, '2025-11-07 10:40:00'::timestamp);

-- Insert Refunds (refunds processed in November 2025, added more)
INSERT INTO payments.refunds (payment_id, order_id, refund_amount, refund_reason, refund_status, refund_method, processed_at, created_at)
SELECT 
    (SELECT payment_id FROM payments.payments WHERE transaction_id = 'TXN-STRIPE-20251025153045'),
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-001'),
    50.00,
    'Partial refund due to late delivery',
    'completed',
    'original_payment',
    '2025-11-04 14:30:00'::timestamp,
    '2025-11-03 10:15:00'::timestamp
UNION ALL
SELECT 
    (SELECT payment_id FROM payments.payments WHERE transaction_id = 'TXN-STRIPE-20251108140000'),
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-007'),
    1163.99,
    'Full refund due to stock unavailability',
    'completed',
    'wallet',
    '2025-11-10 16:45:00'::timestamp,
    '2025-11-10 10:00:00'::timestamp
UNION ALL
SELECT 
    (SELECT payment_id FROM payments.payments WHERE transaction_id = 'TXN-COD-20251112100000'),
    (SELECT order_id FROM orders.orders WHERE order_number = 'ORD-2025-005'),
    58.99,
    'Refund for cancelled order',
    'rejected',
    'bank_transfer',
    NULL,
    '2025-11-12 11:00:00'::timestamp;

CREATE INDEX idx_payments_order ON payments.payments(order_id);
CREATE INDEX idx_payments_status ON payments.payments(payment_status);
CREATE INDEX idx_payments_transaction ON payments.payments(transaction_id);
CREATE INDEX idx_refunds_payment ON payments.refunds(payment_id);
CREATE INDEX idx_refunds_order ON payments.refunds(order_id);
