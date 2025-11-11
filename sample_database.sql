-- DROPPING EXISTING DATABASES IF THEY EXIST
DROP DATABASE IF EXISTS users_db;
DROP DATABASE IF EXISTS products_db;
DROP DATABASE IF EXISTS cart_db;
DROP DATABASE IF EXISTS coupon_db;
DROP DATABASE IF EXISTS wallet_db;
DROP DATABASE IF EXISTS orders_db;
DROP DATABASE IF EXISTS payments_db;

-- =============================================
-- USERS DATABASE
-- =============================================

CREATE DATABASE users_db;

\c users_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE users (
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

CREATE TABLE user_addresses (
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
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
);

-- Insert Users (created between May-November 2025)
INSERT INTO users (email, password_hash, first_name, last_name, phone, date_of_birth, email_verified, created_at) VALUES
('john.doe@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'John', 'Doe', '+1234567890', '1990-05-15', TRUE, '2025-05-12 10:30:00'::timestamp),
('jane.smith@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Jane', 'Smith', '+1234567891', '1985-08-22', TRUE, '2025-06-20 14:15:00'::timestamp),
('mike.johnson@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Mike', 'Johnson', '+1234567892', '1992-03-10', TRUE, '2025-07-08 09:45:00'::timestamp),
('sarah.williams@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'Sarah', 'Williams', '+1234567893', '1988-11-30', FALSE, '2025-09-15 16:20:00'::timestamp),
('david.brown@email.com', '$2b$10$abcdefghijklmnopqrstuv', 'David', 'Brown', '+1234567894', '1995-07-18', TRUE, '2025-10-22 11:00:00'::timestamp);

-- Insert User Addresses
INSERT INTO user_addresses (user_id, address_type, address_line1, city, state, postal_code, country, is_default, created_at)
SELECT 
    user_id, 'both', '123 Main Street', 'New York', 'NY', '10001', 'USA', TRUE, '2025-05-12 10:35:00'::timestamp
FROM users WHERE email = 'john.doe@email.com'
UNION ALL
SELECT 
    user_id, 'shipping', '456 Oak Avenue', 'Brooklyn', 'NY', '11201', 'USA', FALSE, '2025-08-10 15:20:00'::timestamp
FROM users WHERE email = 'john.doe@email.com'
UNION ALL
SELECT 
    user_id, 'both', '789 Pine Road', 'Los Angeles', 'CA', '90001', 'USA', TRUE, '2025-06-20 14:20:00'::timestamp
FROM users WHERE email = 'jane.smith@email.com'
UNION ALL
SELECT 
    user_id, 'both', '321 Elm Street', 'Chicago', 'IL', '60601', 'USA', TRUE, '2025-07-08 09:50:00'::timestamp
FROM users WHERE email = 'mike.johnson@email.com'
UNION ALL
SELECT 
    user_id, 'both', '654 Maple Drive', 'Houston', 'TX', '77001', 'USA', TRUE, '2025-09-15 16:25:00'::timestamp
FROM users WHERE email = 'sarah.williams@email.com'
UNION ALL
SELECT 
    user_id, 'both', '987 Cedar Lane', 'Phoenix', 'AZ', '85001', 'USA', TRUE, '2025-10-22 11:05:00'::timestamp
FROM users WHERE email = 'david.brown@email.com';

CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_user_addresses_user ON user_addresses(user_id);

-- =============================================
-- PRODUCTS DATABASE
-- =============================================

\c postgres
CREATE DATABASE products_db;

\c products_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE categories (
    category_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    category_name VARCHAR(100) NOT NULL,
    parent_category_id UUID,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (parent_category_id) REFERENCES categories(category_id) ON DELETE SET NULL
);

CREATE TABLE products (
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
    FOREIGN KEY (category_id) REFERENCES categories(category_id) ON DELETE SET NULL
);

CREATE TABLE product_images (
    image_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    product_id UUID NOT NULL,
    image_url VARCHAR(500) NOT NULL,
    is_primary BOOLEAN DEFAULT FALSE,
    display_order INTEGER DEFAULT 0,
    FOREIGN KEY (product_id) REFERENCES products(product_id) ON DELETE CASCADE
);

CREATE TABLE product_reviews (
    review_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    product_id UUID NOT NULL,
    user_id UUID NOT NULL,
    rating INTEGER CHECK (rating >= 1 AND rating <= 5),
    review_title VARCHAR(255),
    review_text TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (product_id) REFERENCES products(product_id) ON DELETE CASCADE
);

-- Insert Categories (created in early 2025)
INSERT INTO categories (category_name, description, created_at) VALUES
('Electronics', 'Electronic devices and gadgets', '2025-01-10 08:00:00'::timestamp),
('Clothing', 'Apparel and fashion items', '2025-01-10 08:05:00'::timestamp),
('Books', 'Books and publications', '2025-01-10 08:10:00'::timestamp),
('Home & Kitchen', 'Home appliances and kitchen items', '2025-01-10 08:15:00'::timestamp),
('Sports', 'Sports equipment and accessories', '2025-01-10 08:20:00'::timestamp);

-- Insert Products (added throughout 2025)
INSERT INTO products (category_id, product_name, description, sku, price, cost_price, stock_quantity, created_at, updated_at) 
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
FROM categories WHERE category_name = 'Electronics'
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
FROM categories WHERE category_name = 'Electronics'
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
FROM categories WHERE category_name = 'Clothing'
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
FROM categories WHERE category_name = 'Books'
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
FROM categories WHERE category_name = 'Home & Kitchen'
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
FROM categories WHERE category_name = 'Sports';

-- Insert Product Images
INSERT INTO product_images (product_id, image_url, is_primary, display_order)
SELECT 
    product_id,
    'https://cdn.example.com/images/' || sku || '-1.jpg',
    TRUE,
    1
FROM products;

-- Insert Product Reviews (reviews from October-November 2025)
INSERT INTO product_reviews (product_id, user_id, rating, review_title, review_text, created_at)
SELECT 
    product_id,
    'a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid,
    5,
    'Amazing Sound Quality!',
    'These headphones are incredible. The noise cancellation works perfectly and battery life is excellent.',
    '2025-10-25 15:30:00'::timestamp
FROM products WHERE sku = 'WHP-001'
UNION ALL
SELECT 
    product_id,
    'b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid,
    4,
    'Great smartwatch',
    'Love the features, but wish battery lasted longer.',
    '2025-11-02 10:15:00'::timestamp
FROM products WHERE sku = 'SWU-002'
UNION ALL
SELECT 
    product_id,
    'c3d4e5f6-a7b8-4c5d-0e1f-2a3b4c5d6e7f'::uuid,
    5,
    'Perfect fit',
    'Very comfortable and great quality cotton.',
    '2025-11-08 14:45:00'::timestamp
FROM products WHERE sku = 'CTS-003';

CREATE INDEX idx_products_category ON products(category_id);
CREATE INDEX idx_products_sku ON products(sku);
CREATE INDEX idx_product_images_product ON product_images(product_id);
CREATE INDEX idx_product_reviews_product ON product_reviews(product_id);

-- =============================================
-- CART DATABASE
-- =============================================

\c postgres
CREATE DATABASE cart_db;

\c cart_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE cart (
    cart_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE cart_items (
    cart_item_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    cart_id UUID NOT NULL,
    product_id UUID NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 1 CHECK (quantity > 0),
    added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (cart_id) REFERENCES cart(cart_id) ON DELETE CASCADE
);

-- Insert Carts (active carts from November 2025)
INSERT INTO cart (user_id, created_at, updated_at) VALUES
('a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid, '2025-11-09 10:30:00'::timestamp, '2025-11-09 10:35:00'::timestamp),
('b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid, '2025-11-10 14:20:00'::timestamp, '2025-11-10 15:10:00'::timestamp),
('e5f6a7b8-c9d0-4e1f-2a3b-4c5d6e7f8a9b'::uuid, '2025-11-11 09:00:00'::timestamp, '2025-11-11 09:15:00'::timestamp); -- sarah.williams

-- Insert Cart Items (added in November 2025)
INSERT INTO cart_items (cart_id, product_id, quantity, added_at)
SELECT 
    cart_id,
    'a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d'::uuid,
    1,
    '2025-11-09 10:35:00'::timestamp
FROM cart WHERE user_id = 'a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid
UNION ALL
SELECT 
    cart_id,
    'b2c3d4e5-f6a7-8b9c-0d1e-2f3a4b5c6d7e'::uuid,
    1,
    '2025-11-10 14:30:00'::timestamp
FROM cart WHERE user_id = 'b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid
UNION ALL
SELECT 
    cart_id,
    'c6d7e8f9-a0b1-4c2d-3e4f-5a6b7c8d9e0f'::uuid,
    2,
    '2025-11-10 15:10:00'::timestamp
FROM cart WHERE user_id = 'b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid;

CREATE INDEX idx_cart_user ON cart(user_id);
CREATE INDEX idx_cart_items_cart ON cart_items(cart_id);

-- =============================================
-- COUPON DATABASE
-- =============================================

\c postgres
CREATE DATABASE coupon_db;

\c coupon_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE coupons (
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

CREATE TABLE user_coupon_usage (
    usage_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID NOT NULL,
    coupon_id UUID NOT NULL,
    used_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (coupon_id) REFERENCES coupons(coupon_id) ON DELETE CASCADE
);

-- Insert Coupons (active in November 2025)
INSERT INTO coupons (coupon_code, description, discount_type, discount_value, min_purchase_amount, max_discount_amount, usage_limit, start_date, end_date, is_active, usage_count, created_at) VALUES
('WELCOME10', '10% off on first purchase', 'percentage', 10.00, 50.00, 50.00, 1000, '2025-01-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 245, '2025-01-01 00:00:00'::timestamp),
('SAVE50', 'Flat $50 off on orders above $200', 'fixed', 50.00, 200.00, 50.00, 500, '2025-06-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 87, '2025-06-01 00:00:00'::timestamp),
('BLACKFRIDAY25', '25% off Black Friday sale', 'percentage', 25.00, 100.00, 100.00, 5000, '2025-11-01 00:00:00'::timestamp, '2025-11-30 23:59:59'::timestamp, TRUE, 1523, '2025-10-25 00:00:00'::timestamp),
('FREESHIP', 'Free shipping on all orders', 'fixed', 10.00, 0.00, 10.00, 10000, '2025-01-01 00:00:00'::timestamp, '2025-12-31 23:59:59'::timestamp, TRUE, 3421, '2025-01-01 00:00:00'::timestamp);

-- Insert Coupon Usage (recent usage in October-November 2025)
INSERT INTO user_coupon_usage (user_id, coupon_id, used_at)
SELECT 
    'b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid,
    coupon_id,
    '2025-10-18 15:45:00'::timestamp
FROM coupons WHERE coupon_code = 'WELCOME10'
UNION ALL
SELECT 
    'd4e5f6a7-b8c9-4d0e-1f2a-3b4c5d6e7f8a'::uuid,
    coupon_id,
    '2025-11-05 11:20:00'::timestamp
FROM coupons WHERE coupon_code = 'FREESHIP';

CREATE INDEX idx_coupons_code ON coupons(coupon_code);
CREATE INDEX idx_user_coupon_usage_user ON user_coupon_usage(user_id);

-- =============================================
-- WALLET DATABASE
-- =============================================

\c postgres
CREATE DATABASE wallet_db;

\c wallet_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE wallets (
    wallet_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID UNIQUE NOT NULL,
    balance DECIMAL(10, 2) DEFAULT 0.00 CHECK (balance >= 0),
    currency VARCHAR(3) DEFAULT 'USD',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE wallet_transactions (
    transaction_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    wallet_id UUID NOT NULL,
    transaction_type VARCHAR(20) CHECK (transaction_type IN ('credit', 'debit', 'refund')),
    amount DECIMAL(10, 2) NOT NULL,
    balance_after DECIMAL(10, 2) NOT NULL,
    description TEXT,
    reference_id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (wallet_id) REFERENCES wallets(wallet_id) ON DELETE CASCADE
);

-- Insert Wallets (created with user registration)
INSERT INTO wallets (user_id, balance, created_at, updated_at) VALUES
('a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid, 150.00, '2025-05-12 10:30:00'::timestamp, '2025-11-03 14:20:00'::timestamp),
('b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid, 250.50, '2025-06-20 14:15:00'::timestamp, '2025-10-25 16:30:00'::timestamp),
('c3d4e5f6-a7b8-4c5d-0e1f-2a3b4c5d6e7f'::uuid, 75.25, '2025-07-08 09:45:00'::timestamp, '2025-11-01 10:15:00'::timestamp),
('e5f6a7b8-c9d0-4e1f-2a3b-4c5d6e7f8a9b'::uuid, 0.00, '2025-09-15 16:20:00'::timestamp, '2025-09-15 16:20:00'::timestamp),
('d4e5f6a7-b8c9-4d0e-1f2a-3b4c5d6e7f8a'::uuid, 500.00, '2025-10-22 11:00:00'::timestamp, '2025-11-06 09:45:00'::timestamp);

-- Insert Wallet Transactions (transactions from October-November 2025)
INSERT INTO wallet_transactions (wallet_id, transaction_type, amount, balance_after, description, created_at)
SELECT 
    wallet_id,
    'credit',
    100.00,
    100.00,
    'Welcome bonus credited',
    '2025-05-12 10:35:00'::timestamp
FROM wallets WHERE user_id = 'a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid
UNION ALL
SELECT 
    wallet_id,
    'credit',
    50.00,
    150.00,
    'Referral bonus',
    '2025-11-03 14:20:00'::timestamp
FROM wallets WHERE user_id = 'a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid
UNION ALL
SELECT 
    wallet_id,
    'credit',
    300.00,
    300.00,
    'Account top-up',
    '2025-10-15 12:00:00'::timestamp
FROM wallets WHERE user_id = 'b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid
UNION ALL
SELECT 
    wallet_id,
    'debit',
    49.50,
    250.50,
    'Payment for order ORD-2025-001',
    '2025-10-25 16:30:00'::timestamp
FROM wallets WHERE user_id = 'b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid;

CREATE INDEX idx_wallets_user ON wallets(user_id);
CREATE INDEX idx_wallet_transactions_wallet ON wallet_transactions(wallet_id);

-- =============================================
-- ORDERS DATABASE
-- =============================================

\c postgres
CREATE DATABASE orders_db;

\c orders_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE orders (
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

CREATE TABLE order_items (
    order_item_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    order_id UUID NOT NULL,
    product_id UUID NOT NULL,
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    unit_price DECIMAL(10, 2) NOT NULL,
    total_price DECIMAL(10, 2) NOT NULL,
    FOREIGN KEY (order_id) REFERENCES orders(order_id) ON DELETE CASCADE
);

CREATE TABLE order_status_history (
    history_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    order_id UUID NOT NULL,
    status VARCHAR(20) NOT NULL,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (order_id) REFERENCES orders(order_id) ON DELETE CASCADE
);

-- Insert Orders (orders from October-November 2025)
INSERT INTO orders (user_id, order_number, order_status, subtotal, discount_amount, tax_amount, shipping_amount, total_amount, coupon_id, shipping_address_id, billing_address_id, created_at, updated_at) VALUES
('b2c3d4e5-f6a7-4b5c-9d0e-1f2a3b4c5d6e'::uuid, 'ORD-2025-001', 'delivered', 299.99, 30.00, 24.00, 10.00, 303.99, 'c1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f'::uuid, 'a1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f'::uuid, 'a1d2e3f4-a5b6-4c7d-8e9f-0a1b2c3d4e5f'::uuid, '2025-10-25 15:30:00'::timestamp, '2025-11-02 10:20:00'::timestamp),
('c3d4e5f6-a7b8-4c5d-0e1f-2a3b4c5d6e7f'::uuid, 'ORD-2025-002', 'shipped', 89.99, 0.00, 7.20, 10.00, 107.19, NULL, 'a2d3e4f5-a6b7-4c8d-9e0f-1a2b3c4d5e6f'::uuid, 'a2d3e4f5-a6b7-4c8d-9e0f-1a2b3c4d5e6f'::uuid, '2025-11-06 11:15:00'::timestamp, '2025-11-09 14:30:00'::timestamp),
('d4e5f6a7-b8c9-4d0e-1f2a-3b4c5d6e7f8a'::uuid, 'ORD-2025-003', 'processing', 199.99, 0.00, 16.00, 0.00, 215.99, 'c2d3e4f5-a6b7-4c8d-9e0f-1a2b3c4d5e6f'::uuid, 'a3d4e5f6-a7b8-4c9d-0e1f-2a3b4c5d6e7f'::uuid, 'a3d4e5f6-a7b8-4c9d-0e1f-2a3b4c5d6e7f'::uuid, '2025-11-10 09:45:00'::timestamp, '2025-11-10 16:30:00'::timestamp),
('a1b2c3d4-e5f6-4a5b-8c9d-0e1f2a3b4c5d'::uuid, 'ORD-2025-004', 'confirmed', 139.98, 35.00, 11.20, 10.00, 126.18, 'c3d4e5f6-a7b8-4c9d-0e1f-2a3b4c5d6e7f'::uuid, 'a4d5e6f7-a8b9-4c0d-1e2f-3a4b5c6d7e8f'::uuid, 'a4d5e6f7-a8b9-4c0d-1e2f-3a4b5c6d7e8f'::uuid, '2025-11-11 08:20:00'::timestamp, '2025-11-11 08:35:00'::timestamp);

-- Insert Order Items
INSERT INTO order_items (order_id, product_id, quantity, unit_price, total_price)
SELECT 
    order_id,
    'b2c3d4e5-f6a7-8b9c-0d1e-2f3a4b5c6d7e'::uuid,
    1,
    299.99,
    299.99
FROM orders WHERE order_number = 'ORD-2025-001'
UNION ALL
SELECT 
    order_id,
    'e5f6a7b8-c9d0-4e1f-2a3b-4c5d6e7f8a9b'::uuid,
    1,
    89.99,
    89.99
FROM orders WHERE order_number = 'ORD-2025-002'
UNION ALL
SELECT 
    order_id,
    'a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d'::uuid,
    1,
    199.99,
    199.99
FROM orders WHERE order_number = 'ORD-2025-003'
UNION ALL
SELECT 
    order_id,
    'c3d4e5f6-a7b8-9c0d-1e2f-3a4b5c6d7e8f'::uuid,
    3,
    29.99,
    89.97
FROM orders WHERE order_number = 'ORD-2025-004'
UNION ALL
SELECT 
    order_id,
    'c6d7e8f9-a0b1-4c2d-3e4f-5a6b7c8d9e0f'::uuid,
    1,
    39.99,
    39.99
FROM orders WHERE order_number = 'ORD-2025-004';

-- Insert Order Status History
INSERT INTO order_status_history (order_id, status, notes, created_at)
SELECT order_id, 'pending', 'Order placed', '2025-10-25 15:30:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-001'
UNION ALL
SELECT order_id, 'confirmed', 'Payment confirmed', '2025-10-25 16:15:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-001'
UNION ALL
SELECT order_id, 'processing', 'Order being prepared', '2025-10-26 09:00:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-001'
UNION ALL
SELECT order_id, 'shipped', 'Order shipped via FedEx', '2025-10-28 10:30:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-001'
UNION ALL
SELECT order_id, 'delivered', 'Order delivered successfully', '2025-11-02 10:20:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-001'
UNION ALL
SELECT order_id, 'pending', 'Order placed', '2025-11-06 11:15:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-002'
UNION ALL
SELECT order_id, 'confirmed', 'Payment confirmed', '2025-11-06 11:45:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-002'
UNION ALL
SELECT order_id, 'processing', 'Order being prepared', '2025-11-07 08:30:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-002'
UNION ALL
SELECT order_id, 'shipped', 'Order shipped via UPS', '2025-11-09 14:30:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-002'
UNION ALL
SELECT order_id, 'pending', 'Order placed', '2025-11-10 09:45:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-003'
UNION ALL
SELECT order_id, 'confirmed', 'Payment confirmed via wallet', '2025-11-10 10:00:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-003'
UNION ALL
SELECT order_id, 'processing', 'Order being prepared', '2025-11-10 16:30:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-003'
UNION ALL
SELECT order_id, 'pending', 'Order placed', '2025-11-11 08:20:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-004'
UNION ALL
SELECT order_id, 'confirmed', 'Payment confirmed', '2025-11-11 08:35:00'::timestamp FROM orders WHERE order_number = 'ORD-2025-004';

CREATE INDEX idx_orders_user ON orders(user_id);
CREATE INDEX idx_orders_status ON orders(order_status);
CREATE INDEX idx_orders_number ON orders(order_number);
CREATE INDEX idx_order_items_order ON order_items(order_id);
CREATE INDEX idx_order_status_history_order ON order_status_history(order_id);

-- =============================================
-- PAYMENTS DATABASE
-- =============================================

\c postgres
CREATE DATABASE payments_db;

\c payments_db;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE payments (
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

CREATE TABLE refunds (
    refund_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    payment_id UUID NOT NULL,
    order_id UUID NOT NULL,
    refund_amount DECIMAL(10, 2) NOT NULL,
    refund_reason TEXT,
    refund_status VARCHAR(20) CHECK (refund_status IN ('pending', 'processed', 'completed', 'rejected')),
    refund_method VARCHAR(50) CHECK (refund_method IN ('original_payment', 'wallet', 'bank_transfer')),
    processed_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (payment_id) REFERENCES payments(payment_id) ON DELETE CASCADE
);

-- Insert Payments (payments from October-November 2025)
INSERT INTO payments (order_id, payment_method, payment_status, amount, transaction_id, payment_gateway, payment_date, created_at) VALUES
('01a2d3e4-a5b6-4d7a-8b9c-0d1e2f3a4b5c'::uuid, 'credit_card', 'completed', 303.99, 'TXN-STRIPE-20251025153045', 'Stripe', '2025-10-25 16:15:00'::timestamp, '2025-10-25 15:30:00'::timestamp),
('02a3d4e5-a6b7-4d8a-9b0c-1d2e3f4a5b6c'::uuid, 'upi', 'completed', 107.19, 'TXN-RAZORPAY-20251106111530', 'Razorpay', '2025-11-06 11:45:00'::timestamp, '2025-11-06 11:15:00'::timestamp),
('03a4d5e6-a7b8-4d9a-0b1c-2d3e4f5a6b7c'::uuid, 'wallet', 'completed', 215.99, 'TXN-WALLET-20251110094520', 'Internal', '2025-11-10 10:00:00'::timestamp, '2025-11-10 09:45:00'::timestamp),
('04a5d6e7-a8b9-4d0a-1b2c-3d4e5f6a7b8c'::uuid, 'debit_card', 'completed', 126.18, 'TXN-STRIPE-20251111082035', 'Stripe', '2025-11-11 08:35:00'::timestamp, '2025-11-11 08:20:00'::timestamp);

-- Insert a sample refund (refund processed in November 2025)
INSERT INTO refunds (payment_id, order_id, refund_amount, refund_reason, refund_status, refund_method, processed_at, created_at)
SELECT 
    payment_id,
    '01a2d3e4-a5b6-4d7a-8b9c-0d1e2f3a4b5c'::uuid,
    50.00,
    'Partial refund due to late delivery',
    'completed',
    'original_payment',
    '2025-11-04 14:30:00'::timestamp,
    '2025-11-03 10:15:00'::timestamp
FROM payments WHERE transaction_id = 'TXN-STRIPE-20251025153045';

CREATE INDEX idx_payments_order ON payments(order_id);
CREATE INDEX idx_payments_status ON payments(payment_status);
CREATE INDEX idx_payments_transaction ON payments(transaction_id);
CREATE INDEX idx_refunds_payment ON refunds(payment_id);
CREATE INDEX idx_refunds_order ON refunds(order_id);


